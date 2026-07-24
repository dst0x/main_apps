/**
  ******************************************************************************
  * @file    log_port.c
  * @brief   Wires Middlewares/log to USART2 -- retargets libc's printf/vprintf
  *          (used internally by Log_Put(), see Middlewares/log/src/log.c) to
  *          transmit over USART2 (115200 8N1, PA2), and supplies the RTC
  *          getter Log_Put() needs for its timestamp.
  ******************************************************************************
  */

#include "log_port.h"
#include "log.h"
#include "usart.h"
#include "main.h"

#include <time.h>

/**
  * @brief  No RTC peripheral is configured -- this tracks wall-clock time in
  *         software instead, anchored to HAL_GetTick(). Until LogPort_SetEpoch()
  *         is called at least once (see app_esp32.c, fed from the ESP32's SNTP
  *         time), s_epochOffset is 0 and this is plain seconds-since-boot
  *         (hence the 01/01/1970 timestamps before the first sync).
  */
static int32_t s_epochOffset = 0;

static time_t LogPort_RTCGet(void)
{
  return (time_t)((HAL_GetTick() / 1000U) + (uint32_t)s_epochOffset);
}

void LogPort_SetEpoch(time_t epochNow)
{
  s_epochOffset = (int32_t)((uint32_t)epochNow - (HAL_GetTick() / 1000U));
}

/**
  * @brief  newlib syscall retarget: every printf/vprintf call inside
  *         Log_Put() ends up here. Blocking transmit is fine for debug logs.
  */
int _write(int file, char *ptr, int len)
{
  (void)file;

  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);

  return len;
}

void LogPort_Init(void)
{
  MX_USART2_UART_Init();
  Log_Init(NULL, LogPort_RTCGet);
}
