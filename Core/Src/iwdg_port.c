/**
  ******************************************************************************
  * @file    iwdg_port.c
  * @brief   IWDG bring-up: LSI/256 prescaler, reload 1000 -> ~8 s timeout.
  ******************************************************************************
  */

#include "iwdg_port.h"
#include "main.h"

static IWDG_HandleTypeDef hiwdg;

#define WDG_DELAY_CHUNK_MS  50U

void WDG_Init(void)
{
  hiwdg.Instance       = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Reload    = 1000; /* (1000+1) * 256 / 32000 Hz (LSI) ~= 8.0 s */
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
}

void WDG_Refresh(void)
{
  HAL_IWDG_Refresh(&hiwdg);
}

void WDG_DelayMs(uint32_t ms)
{
  while (ms > WDG_DELAY_CHUNK_MS)
  {
    HAL_Delay(WDG_DELAY_CHUNK_MS);
    WDG_Refresh();
    ms -= WDG_DELAY_CHUNK_MS;
  }

  HAL_Delay(ms);
  WDG_Refresh();
}
