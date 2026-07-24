/**
  ******************************************************************************
  * @file    rtc_port.h
  * @brief   RTC (LSE-clocked calendar) + backup-domain + STANDBY bring-up.
  *          Lets app_bgt persist a wake schedule across STANDBY, where all of
  *          RAM is lost and main() reruns from the reset vector -- only the
  *          RTC calendar and its 20 backup registers survive.
  ******************************************************************************
  */

#ifndef RTC_PORT_H
#define RTC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* App-owned RTC backup register indices (RTC_BKP0R..RTC_BKP19R, 20 total).
 * Index 0 is internal to rtc_port.c (first-boot sentinel). 3-19 reserved. */
#define RTC_BKP_NEXT_BOUNDARY  1U /* target epoch for the next full cycle; 0 = no schedule yet */
#define RTC_BKP_INTERVAL_SEC   2U /* last-known publish interval, seconds; 0 = never received */

/**
  * @brief  Brings up the RTC (LSE 32.768 kHz -> 1 Hz calendar) and backup
  *         domain write access. Idempotent and safe on every boot (cold,
  *         IWDG, or STANDBY-wake) -- HAL_RTC_Init() never touches the
  *         calendar registers itself, so re-running this does not reset the
  *         clock. Only on a genuinely fresh backup domain (first boot ever,
  *         or VBAT was lost) does it seed a placeholder date and zero the
  *         app-owned backup registers. Call once from main(), after
  *         LogPort_Init() (seeding logs) and before any other RTCPort_* call.
  */
void RTCPort_Init(void);

/**
  * @brief  Reads and clears PWR's standby-wake flag on the first call each
  *         boot (it's one-shot hardware state), caching the result for every
  *         later call in the same boot. True if this boot was woken from
  *         STANDBY (by the alarm armed via RTCPort_ArmAlarmAt(), or by IWDG
  *         firing during the sleep -- both look identical from here); false
  *         for a cold boot / NRST / other reset.
  */
uint8_t RTCPort_WasStandbyWake(void);

/**
  * @brief  Current calendar time as a Unix-shaped epoch. Note: this carries
  *         the same GMT+7-baked-in convention as ESP32_ParseTime() (see
  *         Sensors/esp32c3.c) -- it's local wall-clock time wearing a UTC
  *         epoch's clothes, not true UTC. That's intentional: it's what makes
  *         interval-boundary math land on human-meaningful local times.
  */
time_t RTCPort_GetEpoch(void);

/**
  * @brief  Sets the calendar from an epoch in the same GMT+7-baked
  *         representation RTCPort_GetEpoch()/ESP32_ParseTime() use. Year is
  *         clamped to the RTC's 2-digit field (2000-2099).
  */
void RTCPort_SetEpoch(time_t epoch);

/**
  * @brief  Formats epoch (same GMT+7-baked convention as RTCPort_GetEpoch())
  *         as ISO 8601 with an explicit "+07:00" offset, e.g.
  *         "2026-07-24T14:01:00+07:00" -- the offset is required, not
  *         cosmetic: without it the string is ambiguous local-vs-UTC to any
  *         strict downstream consumer. bufSize must be >= 26.
  */
void RTCPort_FormatIso8601(time_t epoch, char *buf, size_t bufSize);

/** Raw backup-register access -- pass RTC_BKP_NEXT_BOUNDARY / RTC_BKP_INTERVAL_SEC. */
uint32_t RTCPort_GetBackup(uint32_t idx);
void     RTCPort_SetBackup(uint32_t idx, uint32_t val);

/**
  * @brief  Arms RTC Alarm A (interrupt-enabled, EXTI17 rising-edge wired) to
  *         match the given epoch's day-of-month/hour/minute/second. Safe to
  *         call again to re-arm for a new target -- deactivates any previous
  *         alarm first. Only the day-of-month is matched (not month/year),
  *         which is fine here since every target this app schedules is at
  *         most a few minutes out, never far enough to need full-date
  *         matching.
  */
void RTCPort_ArmAlarmAt(time_t epoch);

/**
  * @brief  Refreshes the watchdog, clears the standby-wake flag, and enters
  *         STANDBY. Never returns -- the next code to run is main(), from the
  *         reset vector, on whatever wakes the chip (the alarm armed by
  *         RTCPort_ArmAlarmAt(), or IWDG).
  */
void RTCPort_EnterStandby(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_PORT_H */
