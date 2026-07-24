/**
  ******************************************************************************
  * @file    rtc_port.c
  * @brief   RTC (LSE-clocked calendar) + backup-domain + STANDBY bring-up --
  *          see rtc_port.h.
  ******************************************************************************
  */

#include "rtc_port.h"
#include "main.h"
#include "iwdg_port.h"
#include "log.h"

#include <stdio.h>

#define RTC_BKP_SENTINEL_IDX    0U
#define RTC_BKP_SENTINEL_MAGIC  0x42475431UL /* "BGT1" -- marks a backup domain already seeded by this firmware */

static RTC_HandleTypeDef hrtc;
static uint8_t           s_standbyWakeCached = 0xFFU; /* 0xFF = not yet determined this boot */

/**
  * @brief  Howard Hinnant's days-since-1970-01-01 <-> civil-date conversion
  *         pair. Duplicated from the equivalent civil->days routine in
  *         Sensors/esp32c3.c rather than shared, so rtc_port.c stays
  *         independent of the ESP32 driver.
  */
static long DaysFromCivil(int y, int m, int d)
{
  long     era;
  unsigned yoe;
  unsigned doy;
  unsigned doe;

  y -= (m <= 2) ? 1 : 0;
  era = ((y >= 0) ? y : (y - 399)) / 400;
  yoe = (unsigned)(y - (era * 400));
  doy = (unsigned)(((153 * (m + ((m > 2) ? -3 : 9))) + 2) / 5 + d - 1);
  doe = (yoe * 365U) + (yoe / 4U) - (yoe / 100U) + doy;

  return (era * 146097L) + (long)doe - 719468L;
}

static void CivilFromDays(long z, int *outY, int *outM, int *outD)
{
  long     era;
  unsigned doe;
  unsigned yoe;
  unsigned doy;
  unsigned mp;
  int      y;

  z += 719468L;
  era = (z >= 0L) ? (z / 146097L) : ((z - 146096L) / 146097L);
  doe = (unsigned)(z - (era * 146097L));
  yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
  y   = (int)((long)yoe + (era * 400L));
  doy = doe - ((365U * yoe) + (yoe / 4U) - (yoe / 100U));
  mp  = ((5U * doy) + 2U) / 153U;

  *outD = (int)(doy - (((153U * mp) + 2U) / 5U) + 1U);
  *outM = (int)(mp + ((mp < 10U) ? 3U : (uint32_t)(-9)));
  *outY = y + ((*outM <= 2) ? 1 : 0);
}

/** Epoch (as used throughout this codebase, see rtc_port.h) -> civil day count + seconds-of-day. */
static void EpochToDaysAndSeconds(time_t epoch, long *outDays, long *outSecOfDay)
{
  long days = (long)(epoch / 86400L);
  long sec  = (long)(epoch % 86400L);

  if (sec < 0L)
  {
    sec  += 86400L;
    days -= 1L;
  }

  *outDays    = days;
  *outSecOfDay = sec;
}

void RTCPort_Init(void)
{
  RCC_OscInitTypeDef       oscInit  = {0};
  RCC_PeriphCLKInitTypeDef pclkInit = {0};

  HAL_PWR_EnableBkUpAccess();

  oscInit.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  oscInit.LSEState       = RCC_LSE_ON;
  if (HAL_RCC_OscConfig(&oscInit) != HAL_OK)
  {
    Error_Handler();
  }

  pclkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  pclkInit.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&pclkInit) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_RTC_ENABLE();

  hrtc.Instance            = RTC;
  hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv   = 127U;  /* 32.768kHz / 128 / 256 = 1Hz */
  hrtc.Init.SynchPrediv    = 255U;
  hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* Shadow registers (what HAL_RTC_GetTime/GetDate actually read) go stale
   * across RTC domain reset and, critically, EVERY STANDBY wake -- without
   * this, RTCPort_GetEpoch() can silently return a frozen/incorrect value
   * for up to 2 RTCCLK cycles after each boot, which is exactly what a nap
   * chain hits on every single link. */
  (void)HAL_RTC_WaitForSynchro(&hrtc);

  /* RTC Alarm A -> EXTI17, needed for the alarm to wake STANDBY (not just an
   * ISR while running). One-time wiring, safe to redo every boot. */
  __HAL_RTC_ALARM_EXTI_ENABLE_IT();
  __HAL_RTC_ALARM_EXTI_ENABLE_RISING_EDGE();

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_SENTINEL_IDX) != RTC_BKP_SENTINEL_MAGIC)
  {
    LOG_WARNING("RTC: fresh backup domain, seeding placeholder time + schedule");

    RTCPort_SetEpoch((time_t)1704067200L); /* 2024-01-01 00:00:00, placeholder until first NTP sync */
    RTCPort_SetBackup(RTC_BKP_NEXT_BOUNDARY, 0U);
    RTCPort_SetBackup(RTC_BKP_INTERVAL_SEC, 0U);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_SENTINEL_IDX, RTC_BKP_SENTINEL_MAGIC);
  }
}

uint8_t RTCPort_WasStandbyWake(void)
{
  if (s_standbyWakeCached == 0xFFU)
  {
    s_standbyWakeCached = (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) ? 1U : 0U;
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
  }

  return s_standbyWakeCached;
}

time_t RTCPort_GetEpoch(void)
{
  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;
  long             days;

  /* Must read Time then Date (in that order) to correctly unlatch the
   * calendar shadow registers -- an HAL/silicon requirement, not a style choice. */
  (void)HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  (void)HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

  days = DaysFromCivil(2000 + (int)date.Year, (int)date.Month, (int)date.Date);

  return (time_t)((days * 86400L) + ((long)time.Hours * 3600L)
                   + ((long)time.Minutes * 60L) + (long)time.Seconds);
}

void RTCPort_SetEpoch(time_t epoch)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  long             days;
  long             secOfDay;
  int              y;
  int              m;
  int              d;
  long             dow; /* 0=Monday .. 6=Sunday; days=0 (1970-01-01) was a Thursday */

  EpochToDaysAndSeconds(epoch, &days, &secOfDay);
  CivilFromDays(days, &y, &m, &d);

  if (y < 2000) { y = 2000; }
  if (y > 2099) { y = 2099; }

  dow = ((days % 7L) + 7L + 3L) % 7L;

  date.Year    = (uint8_t)(y - 2000);
  date.Month   = (uint8_t)m;
  date.Date    = (uint8_t)d;
  date.WeekDay = (uint8_t)(dow + 1L); /* RTC_WEEKDAY_MONDAY(1) .. RTC_WEEKDAY_SUNDAY(7) */

  time.Hours   = (uint8_t)(secOfDay / 3600L);
  time.Minutes = (uint8_t)((secOfDay % 3600L) / 60L);
  time.Seconds = (uint8_t)(secOfDay % 60L);

  (void)HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
  (void)HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
}

void RTCPort_FormatIso8601(time_t epoch, char *buf, size_t bufSize)
{
  long days;
  long secOfDay;
  int  y;
  int  m;
  int  d;

  EpochToDaysAndSeconds(epoch, &days, &secOfDay);
  CivilFromDays(days, &y, &m, &d);

  (void)snprintf(buf, bufSize, "%04d-%02d-%02dT%02ld:%02ld:%02ld+07:00",
                 y, m, d,
                 secOfDay / 3600L, (secOfDay % 3600L) / 60L, secOfDay % 60L);
}

uint32_t RTCPort_GetBackup(uint32_t idx)
{
  return HAL_RTCEx_BKUPRead(&hrtc, idx);
}

void RTCPort_SetBackup(uint32_t idx, uint32_t val)
{
  HAL_RTCEx_BKUPWrite(&hrtc, idx, val);
}

void RTCPort_ArmAlarmAt(time_t epoch)
{
  RTC_AlarmTypeDef alarm = {0};
  long              days;
  long              secOfDay;
  int               y;
  int               m;
  int               d;

  EpochToDaysAndSeconds(epoch, &days, &secOfDay);
  CivilFromDays(days, &y, &m, &d);
  (void)y;
  (void)m;

  alarm.AlarmTime.Hours          = (uint8_t)(secOfDay / 3600L);
  alarm.AlarmTime.Minutes        = (uint8_t)((secOfDay % 3600L) / 60L);
  alarm.AlarmTime.Seconds        = (uint8_t)(secOfDay % 60L);
  alarm.AlarmMask                = RTC_ALARMMASK_NONE;
  alarm.AlarmSubSecondMask       = RTC_ALARMSUBSECONDMASK_ALL;
  alarm.AlarmDateWeekDaySel      = RTC_ALARMDATEWEEKDAYSEL_DATE;
  alarm.AlarmDateWeekDay         = (uint8_t)d;
  alarm.Alarm                    = RTC_ALARM_A;

  (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  (void)HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN);
}

void RTCPort_EnterStandby(void)
{
  WDG_Refresh();
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
  HAL_PWR_EnterSTANDBYMode();

  for (;;)
  {
    /* Unreachable -- STANDBY entry resets the chip on wake. */
  }
}
