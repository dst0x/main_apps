/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt service routines
  ******************************************************************************
  */

#include "main.h"
#include "stm32f4xx_it.h"
#include "usart.h"
#include "tx_api.h"
#include "app_threadx.h"

#if defined(APP_ESP32)
#include "app_esp32/app_esp32.h"
#elif defined(APP_BGT)
#include "app_bgt/app_bgt.h"
#endif

/* Provided by ThreadX (tx_timer_interrupt.S); drives tx_thread_sleep(),
 * timeouts, time-slicing, etc. */
extern void _tx_timer_interrupt(void);

void NMI_Handler(void)
{
  /* CSS raises NMI on HSE failure; HAL_RCC_NMI_IRQHandler clears RCC_CIR_CSSF
   * and invokes HAL_RCC_CSSCallback() below */
  HAL_RCC_NMI_IRQHandler();
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

/* PendSV_Handler is intentionally not defined here: ThreadX's Cortex-M4/GNU
 * port (Middlewares/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S)
 * provides its own strong PendSV_Handler for context switching -- defining
 * one here too would be a duplicate-symbol link error. */

void SysTick_Handler(void)
{
  HAL_IncTick();

  /* HAL_Init() (main()) starts SysTick ticking long before tx_kernel_enter()
   * initializes ThreadX's timer subsystem; forwarding ticks before that is
   * ready corrupts _tx_timer_current_ptr and friends (see app_threadx.h). */
  if (g_ThreadXTickReady != 0U)
  {
    _tx_timer_interrupt();
  }
}

/**
  * @brief  Called by HAL_RCC_NMI_IRQHandler() after CSS fires, i.e. HSE has
  *         died and SYSCLK has already been auto-switched to HSI by hardware.
  */
void HAL_RCC_CSSCallback(void)
{
  Error_Handler();
}

/* ---- Modbus RTU transport (USART1 + DMA2 Stream2/7) --------------------- */

void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

void DMA2_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void DMA2_Stream7_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/* ---- ESP32-C3 AT transport (USART6, interrupt-only, no DMA) ------------- */

#if defined(APP_ESP32) || defined(APP_BGT)
void USART6_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart6);
}
#endif

/**
  * @brief  Fired by the HAL either when the RX buffer is full or, more
  *         usually, when the line goes IDLE after the peer's response --
  *         Size is the actual number of bytes received. app_bgt drives both
  *         USART1 (Modbus) and USART6 (ESP-AT) concurrently, so dispatch by
  *         the UART instance rather than by build macro.
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
#if defined(APP_BGT)
  if (huart->Instance == USART1)
  {
    Modbus_RxEventCallback(&hmodbusSensor, huart, Size);
  }
  else
  {
    ESPAT_RxEventCallback(&hEspAt, huart, Size);
  }
#elif defined(APP_ESP32)
  ESPAT_RxEventCallback(&hEspAt, huart, Size);
#endif
}
