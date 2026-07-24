/**
  ******************************************************************************
  * @file    iwdg_port.h
  * @brief   Independent Watchdog bring-up + a "wait without hanging" helper.
  *          ~8 s timeout (LSI/256, reload 1000) -- long enough for a slow
  *          Modbus/AT round trip, short enough to actually catch a real hang.
  ******************************************************************************
  */

#ifndef IWDG_PORT_H
#define IWDG_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
  * @brief  Starts the IWDG. Call once at startup. NOTE: once started, IWDG
  *         can only be stopped by a reset -- every code path from this point
  *         on must call WDG_Refresh() (directly, or via WDG_DelayMs()) at
  *         least every ~8 s or the MCU will reset.
  */
void WDG_Init(void);

/**
  * @brief  Pets the watchdog. Call once per iteration of every app's main
  *         loop, and periodically from inside any wait that can take a while
  *         (see WDG_DelayMs()).
  */
void WDG_Refresh(void);

/**
  * @brief  A HAL_Delay() that can't starve the watchdog: sleeps in small
  *         chunks and calls WDG_Refresh() between them, so a single long
  *         "wait for X" doesn't need its own watchdog bookkeeping.
  */
void WDG_DelayMs(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* IWDG_PORT_H */
