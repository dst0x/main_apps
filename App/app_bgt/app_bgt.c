#include "app_bgt.h"
#include "../../Sensors/sensor_bgt_w87x/bgt_w87x.h"
#include "esp32c3.h"
#include "usart.h"
#include "iwdg_port.h"
#include "log.h"
#include "log_port.h"
#include "rtc_port.h"

#include <stdio.h>

#define WIFI_SSID           "hallo dek"
#define WIFI_PASSWORD       "123456780"
#define MQTT_CLIENT_ID      "AGV-BSJA829JSKS"
#define MQTT_USERNAME       "agriva_inovasi"
#define MQTT_PASSWORD       "D4ni4455Ty$"
#define MQTT_BROKER_HOST    "broker.avisha.id"
#define MQTT_BROKER_PORT    1883U
#define MQTT_TOPIC_RECORD   "agriva_inovasi/record"
#define MQTT_TOPIC_INTERVAL "agriva_inovasi/interval"

#define APP_BGT_SLAVE_ADDR   8U

#define APP_BGT_STACK_SIZE    4096U
#define APP_BGT_PRIORITY      10U
#define APP_BGT_RETRY_MS      3000U
#define APP_BGT_ESP_WARMUP_MS 3000U
#define APP_BGT_DEFAULT_INTERVAL_SEC  60U /* used only if nothing was ever persisted and no subscribe reply arrives in time */

static TX_THREAD       appBgtThread;
static BGT_Data_t      data;

ESPAT_HandleTypeDef hEspAt;

static void AppBgt_WaitAndRetry(const char *what){
  LOG_ERROR("ESP32: %s failed, retrying in %u ms", what, (unsigned int)APP_BGT_RETRY_MS);
  WDG_DelayMs(APP_BGT_RETRY_MS);
}

static void AppBgt_Entry(ULONG initial_input){
  char     payload[224];
  char     timeStr[48];
  char     isoTime[32];
  int16_t  rssi;
  int32_t  intervalSeconds;
  uint32_t intervalSec;
  time_t   nowEpoch;
  time_t   nextBoundary;

  (void)initial_input;

  WDG_Init();

  W87X_Init(APP_BGT_SLAVE_ADDR);

  MX_USART6_UART_Init();
  ESPAT_Init(&hEspAt, &huart6, WDG_Refresh);

  /* ESP32_UartResync() call removed for now -- it correlated 1:1 with a
   * reproducible IWDG boot-loop (reset -> "warming up" log -> reset again,
   * every single time, no AT attempt ever logged). Root cause not confirmed;
   * revisit with WDG_Refresh() bracketing the blocking UART write and/or a
   * much smaller burst before re-enabling. See esp32c3.c/.h for the function. */

  /* ESP32 never power-cycles when only the STM32 resets/wakes from STANDBY
   * (separate chip, separate power domain) -- only skip the warmup wait on a
   * standby-wake cycle, where it's already fully booted from before. */
  if (!RTCPort_WasStandbyWake()){
    LOG_INFO("ESP32: warming up (%u ms)...", (unsigned int)APP_BGT_ESP_WARMUP_MS);
    WDG_DelayMs(APP_BGT_ESP_WARMUP_MS);
  }

  while (ESP32_Init(&hEspAt) != ESP32_OK){
    AppBgt_WaitAndRetry("AT handshake");
  }
  LOG_INFO("AT handshake OK");

  while (ESP32_ConnectWiFi(&hEspAt, WIFI_SSID, WIFI_PASSWORD, 20000U) != ESP32_OK){
    AppBgt_WaitAndRetry("WiFi connect");
  }
  LOG_INFO("WiFi connected");

  while (ESP32_MQTT_Connect(&hEspAt, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                             MQTT_BROKER_HOST, MQTT_BROKER_PORT, 10000U)
         != ESP32_OK){
    AppBgt_WaitAndRetry("MQTT connect");
  }
  LOG_INFO("MQTT connected (%s:%u)", MQTT_BROKER_HOST, (unsigned int)MQTT_BROKER_PORT);

  /* NTP sync -- reuses the proven ESP32_GetTime()/ESP32_ParseTime() pattern
   * from App/app_esp32/app_esp32.c. Feeds both the hardware RTC (source of
   * truth for alarm scheduling, survives STANDBY) and the software log clock
   * (cheap, just for this cycle's remaining log timestamps). */
  if (ESP32_GetTime(&hEspAt, timeStr, sizeof(timeStr)) == ESP32_OK){
    time_t epoch;
    if (ESP32_ParseTime(timeStr, &epoch) == ESP32_OK){
      RTCPort_SetEpoch(epoch);
      LogPort_SetEpoch(epoch);
      LOG_INFO("NTP sync OK: %s", timeStr);
    }else{
      LOG_WARNING("NTP: couldn't parse time string \"%s\"", timeStr);
    }
  }else{
    LOG_WARNING("NTP: GetTime failed, keeping previous RTC time");
  }

  if (ESP32_MQTT_SubscribeAndGetInterval(&hEspAt, MQTT_TOPIC_INTERVAL, 1U, 5000U, 4000U,
                                          &intervalSeconds) == ESP32_OK
      && intervalSeconds >= 1){
    RTCPort_SetBackup(RTC_BKP_INTERVAL_SEC, (uint32_t)intervalSeconds);
    LOG_INFO("Interval: %ld s (from %s)", (long)intervalSeconds, MQTT_TOPIC_INTERVAL);
  }else{
    LOG_WARNING("Interval: no valid value from %s, using last-known/default", MQTT_TOPIC_INTERVAL);
  }

  intervalSec = RTCPort_GetBackup(RTC_BKP_INTERVAL_SEC);
  if (intervalSec == 0U){
    intervalSec = APP_BGT_DEFAULT_INTERVAL_SEC;
  }

  if (W87X_GetData(&data) == BGT_OK){
    LOG_INFO("Temperature: %.1fC Humidity: %.1f%% Pressure: %.1fhPa Wind Speed: %.2fm/s Wind Direction: %u deg",
             data.temperature, data.humidity, data.pressure,
             data.wind_speed, data.wind_direction);
  }else{
    LOG_ERROR("Read BGT W87X Failed");
  }

  if (ESP32_GetRSSI(&hEspAt, &rssi) != ESP32_OK){
    LOG_WARNING("ESP32: GetRSSI failed");
    rssi = 0;
  }

  nowEpoch = RTCPort_GetEpoch();
  RTCPort_FormatIso8601(nowEpoch, isoTime, sizeof(isoTime));

  (void)snprintf(payload, sizeof(payload),
                 "{\"time\":\"%s\",\"rssi\":%d,"
                 "\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f,"
                 "\"wind_speed\":%.2f,\"wind_direction\":%u}",
                 isoTime, (int)rssi,
                 (double)data.temperature, (double)data.humidity,
                 (double)data.pressure, (double)data.wind_speed,
                 (unsigned int)data.wind_direction);

  if (ESP32_MQTT_Publish(&hEspAt, MQTT_TOPIC_RECORD, payload, 0U, 0U, 5000U) == ESP32_OK){
    LOG_INFO("Published to %s: %s", MQTT_TOPIC_RECORD, payload);
  }else{
    LOG_ERROR("MQTT publish failed");
  }

  /* Schedule the next wall-clock-aligned wake and go to sleep. Never returns --
   * STANDBY wake reruns main() from the reset vector. */
  nextBoundary = (time_t)(((nowEpoch / (time_t)intervalSec) + 1) * (time_t)intervalSec);
  RTCPort_SetBackup(RTC_BKP_NEXT_BOUNDARY, (uint32_t)nextBoundary);
  // LOG_INFO("Next cycle in %ld s, entering STANDBY", (long)(nextBoundary - nowEpoch));

  RTCPort_ArmAlarmAt(nextBoundary);
  RTCPort_EnterStandby();
}

UINT App_BGT_Init(VOID *memory_ptr){
  TX_BYTE_POOL *bytePool = (TX_BYTE_POOL *)memory_ptr;
  CHAR *stackPtr = TX_NULL;

  if (tx_byte_allocate(bytePool, (VOID **)&stackPtr, APP_BGT_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS){
    return TX_POOL_ERROR;
  }

  return tx_thread_create(&appBgtThread, "App BGT", AppBgt_Entry, 0U,
                           stackPtr, APP_BGT_STACK_SIZE,
                           APP_BGT_PRIORITY, APP_BGT_PRIORITY,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
}
