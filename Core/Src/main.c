/**
  ******************************************************************************
  * @file    main.c
  * @brief   Clock tree bring-up for STM32F412RETx
  *          HSE 8 MHz -> PLLM/4 -> PLLN x96 -> PLLP/2 = SYSCLK 96 MHz
  *          AHB /1 = 96 MHz, APB1 /2 = 48 MHz, APB2 /1 = 96 MHz
  *          PLLI2S -> 48 MHz I2S clock, CSS enabled on HSE
  ******************************************************************************
  */

#include "main.h"
#include "usart.h"
#include "app_threadx.h"
#include "log_port.h"
#include "log.h"
#include "rtc_port.h"
#include "iwdg_port.h"

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void LogResetCause(void);

/* Modbus RTU master bound to USART1; see HAL_UARTEx_RxEventCallback()
 * in stm32f4xx_it.c for how responses reach this handle, and
 * App/app_threadx.c for the thread that actually polls the BGT-W87x sensor. */
Modbus_HandleTypeDef hmodbusSensor;

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  LogPort_Init(); /* USART2, 115200 8N1 -- LOG_INFO/WARNING/ERROR from here on */
  (void)RTCPort_WasStandbyWake(); /* reads+clears+caches PWR_FLAG_SB early, before LogResetCause() reports it */
  LogResetCause();
  RTCPort_Init();

  /* Log timestamps use a RAM-only clock (log_port.c) that's wiped on every
   * reset, including a STANDBY wake -- unlike the RTC itself, which is
   * backup-domain-protected and survives STANDBY fine. Feed it from the RTC
   * immediately so log lines are dated correctly from the very first one,
   * not just after this cycle's own NTP resync (which still runs later and
   * corrects for any drift). */
  LogPort_SetEpoch(RTCPort_GetEpoch());

  /* Nap-chain gate: STANDBY wipes RAM, so every wake reruns main() from here.
   * If we're not yet due for a full read+publish cycle, skip USART1/Modbus/
   * ThreadX/WiFi entirely and go straight back to sleep -- IWDG can't be
   * paused or extended past ~8s on this MCU (see rtc_port.h), so a long
   * interval is a chain of short naps, each re-checking the persisted target
   * time, rather than one continuous STANDBY call. */
  {
    uint32_t nextBoundary = RTCPort_GetBackup(RTC_BKP_NEXT_BOUNDARY);
    time_t   now          = RTCPort_GetEpoch();

    if ((nextBoundary != 0U) && (now < (time_t)nextBoundary))
    {
      // LOG_INFO("Nap: now=%ld target=%lu, not due for %ld s, back to STANDBY",
      //          (long)now, (unsigned long)nextBoundary, (long)((time_t)nextBoundary - now));
      RTCPort_ArmAlarmAt((time_t)nextBoundary);
      WDG_Init(); /* fresh ~8s IWDG safety net for this nap link */
      RTCPort_EnterStandby(); /* never returns */
    }
  }

  /* Due now, or first-ever boot with no schedule yet -- full cycle. */
  MX_USART1_UART_Init();
  Modbus_Init(&hmodbusSensor, &huart1, 200U); /* 200 ms response timeout */
  MX_ThreadX_Init(); /* enters the ThreadX kernel -- never returns */
}

/**
  * @brief  Configure SYSCLK/HCLK/PCLKx, PLLI2S and the Clock Security System.
  *
  * PLLI2S note: on STM32F412 the I2S PLL's PLLI2SSSRC bit only chooses between
  * "same oscillator as the main PLL" or "external I2S_CKIN pin" -- it cannot
  * independently run PLLI2S from HSI while the main PLL runs from HSE. Since
  * the main PLL below is sourced from HSE, PLLI2S is configured from HSE too
  * (PLLI2SM/N/R chosen to still land on a 48 MHz I2S clock). STM32F412 also has
  * no PLLI2SP output (only M/N/Q/R) -- PLLI2SR is used here instead of "PLLI2SP".
  */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /* VOS Scale 1 required to run HCLK at 96 MHz on STM32F412 */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* HSE 8 MHz -> /PLLM=4 -> 2 MHz VCO input -> xPLLN=96 -> 192 MHz VCO
   * -> /PLLP=2 -> SYSCLK = 96 MHz */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 4;
  RCC_OscInitStruct.PLL.PLLN       = 96;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 4;   /* 192/4 = 48 MHz, spare for USB FS/SDIO/RNG */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* HCLK = SYSCLK/1 = 96 MHz, PCLK1 = HCLK/2 = 48 MHz, PCLK2 = HCLK/1 = 96 MHz.
   * Flash latency 3WS is required for 96 MHz at VOS Scale 1 / 3.3V. */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /* PLLI2S, sourced from HSE via the shared PLLSRC select (see note above):
   * 8 MHz /PLLI2SM=4 -> 2 MHz -> xPLLI2SN=96 -> 192 MHz VCO -> /PLLI2SR=4 -> 48 MHz */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_PLLI2S
                                            | RCC_PERIPHCLK_I2S_APB1
                                            | RCC_PERIPHCLK_I2S_APB2;
  PeriphClkInitStruct.PLLI2S.PLLI2SM = 4;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 96;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 4;
  PeriphClkInitStruct.PLLI2S.PLLI2SQ = 2;
  PeriphClkInitStruct.PLLI2SSelection      = RCC_PLLI2SCLKSOURCE_PLLSRC;
  PeriphClkInitStruct.I2sApb1ClockSelection = RCC_I2SAPB1CLKSOURCE_PLLI2S;
  PeriphClkInitStruct.I2sApb2ClockSelection = RCC_I2SAPB2CLKSOURCE_PLLI2S;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Clock Security System: on HSE failure, hardware auto-switches SYSCLK to
   * HSI and raises the CSS interrupt through NMI_Handler() in stm32f4xx_it.c */
  HAL_RCC_EnableCSS();

  /* ART accelerator: safe/standard at any flash latency > 0WS, matches the
   * PREFETCH_ENABLE/INSTRUCTION_CACHE_ENABLE/DATA_CACHE_ENABLE macros already
   * defined in stm32f4xx_hal_conf.h. */
#if (PREFETCH_ENABLE != 0)
  __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
#endif
#if (INSTRUCTION_CACHE_ENABLE != 0)
  __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif
#if (DATA_CACHE_ENABLE != 0)
  __HAL_FLASH_DATA_CACHE_ENABLE();
#endif
}

static void MX_GPIO_Init(void)
{
  /* No peripherals configured yet -- add GPIO clock enables / HAL_GPIO_Init()
   * calls here once the application pinout is defined. */
}

/**
  * @brief  Logs why the MCU last reset (RCC_CSR reset flags), then clears
  *         them so the next reset cycle reports fresh. Diagnoses silent
  *         resets, e.g. an IWDG timeout from a hung fault handler, that
  *         would otherwise just look like "the board restarted" with no clue why.
  */
static void LogResetCause(void)
{
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
  {
    // LOG_ERROR("Reset cause: IWDG timeout (something hung without WDG_Refresh())");
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
  {
    // LOG_ERROR("Reset cause: window watchdog");
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
  {
    // LOG_INFO("Reset cause: software reset");
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
  {
    // LOG_INFO("Reset cause: power-on/power-down reset");
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
  {
    // LOG_INFO("Reset cause: NRST pin (external/manual reset)");
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
  {
    // LOG_INFO("Reset cause: low-power");
  }
  else
  {
    // LOG_INFO("Reset cause: unknown");
  }

  if (RTCPort_WasStandbyWake())
  {
    // LOG_INFO("Reset cause: woke from STANDBY (RTC alarm or IWDG during sleep)");
  }

  __HAL_RCC_CLEAR_RESET_FLAGS();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
