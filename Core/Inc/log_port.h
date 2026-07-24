/**
  ******************************************************************************
  * @file    log_port.h
  * @brief   Wires Middlewares/log (LOG_INFO/LOG_WARNING/LOG_ERROR) to USART2.
  ******************************************************************************
  */

#ifndef LOG_PORT_H
#define LOG_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

/**
  * @brief  Brings up USART2 (115200 8N1) and initializes Middlewares/log.
  *         Call once from main(), before any LOG_INFO/LOG_WARNING/LOG_ERROR.
  */
void LogPort_Init(void);

/**
  * @brief  Feeds a real wall-clock reading (e.g. from ESP32_ParseTime() on an
  *         SNTP result) into the log timestamp. Until this is called at
  *         least once, log lines are stamped with seconds-since-boot
  *         (starting at 01/01/1970). Safe to call repeatedly -- each call
  *         re-anchors the clock to HAL_GetTick(), which both corrects drift
  *         and re-syncs after a reset.
  */
void LogPort_SetEpoch(time_t epochNow);

#ifdef __cplusplus
}
#endif

#endif /* LOG_PORT_H */
