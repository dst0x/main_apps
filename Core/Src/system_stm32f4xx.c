/**
  ******************************************************************************
  * @file    system_stm32f4xx.c
  * @brief   CMSIS system source file for STM32F412RETx.
  *          Only resets RCC to its power-on-reset state and relocates the
  *          vector table -- the real clock tree (96 MHz PLL) is configured
  *          later by SystemClock_Config() in main.c.
  ******************************************************************************
  */

#include "stm32f4xx.h"

#define VECT_TAB_OFFSET  0x00U /* vector table sits right at the start of FLASH */

uint32_t SystemCoreClock = 16000000U; /* HSI reset default; corrected by HAL_RCC_ClockConfig() */

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  /* Set CP10 and CP11 to Full Access */
  SCB->CPACR |= ((3UL << 10U * 2U) | (3UL << 11U * 2U));
#endif

  /* Put RCC back to its power-on-reset state so SystemClock_Config() always
   * starts from a known baseline, regardless of what a debugger or a prior
   * soft-reset left behind. */
  RCC->CR |= RCC_CR_HSION;
  RCC->CFGR = 0x00000000U;
  RCC->CR &= 0xFEF6FFFFU;      /* clear HSEON, CSSON, PLLON */
  RCC->PLLCFGR = 0x24003010U; /* reset value */
  RCC->CR &= 0xFFFBFFFFU;      /* clear HSEBYP */
  RCC->CIR = 0x00000000U;      /* disable all RCC interrupt sources/flags */

  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
}

void SystemCoreClockUpdate(void)
{
  uint32_t tmp;
  uint32_t pllvco;
  uint32_t pllp;
  uint32_t pllm;

  tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp)
  {
    case RCC_CFGR_SWS_HSE:
      SystemCoreClock = HSE_VALUE;
      break;

    case RCC_CFGR_SWS_PLL:
      pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
      if ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) != 0U)
      {
        pllvco = (HSE_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U);
      }
      else
      {
        pllvco = (HSI_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U);
      }
      pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 16U) + 1U) * 2U;
      SystemCoreClock = pllvco / pllp;
      break;

    case RCC_CFGR_SWS_HSI:
    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }

  tmp = AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
  SystemCoreClock >>= tmp;
}
