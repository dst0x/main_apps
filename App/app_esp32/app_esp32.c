#include "app_esp32.h"
#include "esp32c3.h"
#include "usart.h"
#include "iwdg_port.h"
#include "log.h"
#include "log_port.h"

#include <stdio.h>
#include <string.h>

#define WIFI_SSID           "hallo dek"
#define WIFI_PASSWORD       "123456780"
#define MQTT_CLIENT_ID      "stm32-esp32c3"
#define MQTT_USERNAME       "agriva_inovasi"
#define MQTT_PASSWORD       "D4ni4455Ty$"
#define MQTT_BROKER_HOST    "broker.avisha.id"
#define MQTT_BROKER_PORT    1883U
#define MQTT_TOPIC          "agriva_inovasi/signal"

#define APP_ESP32_STACK_SIZE   4096U /* AT parsing + snprintf use more stack than the BGT app */
#define APP_ESP32_PRIORITY     10U
#define APP_ESP32_PUBLISH_MS   5000U /* one get-time/RSSI/publish cycle every 5 s */
#define APP_ESP32_RETRY_MS     3000U /* pause between retries after a failed step */

static TX_THREAD appEsp32Thread;

ESPAT_HandleTypeDef hEspAt;

static void AppEsp32_WaitAndRetry(const char *what)
{
  LOG_ERROR("ESP32: %s failed, retrying in %u ms", what, (unsigned int)APP_ESP32_RETRY_MS);
  WDG_DelayMs(APP_ESP32_RETRY_MS);
}

static void AppEsp32_Entry(ULONG initial_input)
{
  char    timeStr[48];
  int16_t rssi;
  char    payload[192];

  (void)initial_input;

  MX_USART6_UART_Init();
  ESPAT_Init(&hEspAt, &huart6, WDG_Refresh); /* WDG_Refresh() pets the dog during long AT waits */
  WDG_Init();

  /* 1. Basic AT handshake -- retry forever, module may still be booting. */
  while (ESP32_Init(&hEspAt) != ESP32_OK)
  {
    AppEsp32_WaitAndRetry("AT handshake");
  }
  LOG_INFO("ESP32: AT handshake OK");

  /* 2. Join WiFi. */
  while (ESP32_ConnectWiFi(&hEspAt, WIFI_SSID, WIFI_PASSWORD, 20000U) != ESP32_OK)
  {
    AppEsp32_WaitAndRetry("WiFi connect");
  }
  LOG_INFO("ESP32: WiFi connected (SSID \"%s\")", WIFI_SSID);

  /* 3. Connect to the MQTT broker. */
  while (ESP32_MQTT_Connect(&hEspAt, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                             MQTT_BROKER_HOST, MQTT_BROKER_PORT, 10000U)
         != ESP32_OK)
  {
    AppEsp32_WaitAndRetry("MQTT connect");
  }
  LOG_INFO("ESP32: MQTT connected (%s:%u)", MQTT_BROKER_HOST, (unsigned int)MQTT_BROKER_PORT);

  /* 4. Get time + RSSI, publish, repeat -- refreshing the watchdog every
   *    cycle on top of the AT engine's own petting during each call. */
  for (;;)
  {
    ESP32_StatusTypeDef timeStatus = ESP32_GetTime(&hEspAt, timeStr, sizeof(timeStr));
    ESP32_StatusTypeDef rssiStatus = ESP32_GetRSSI(&hEspAt, &rssi);

    if (timeStatus != ESP32_OK)
    {
      strncpy(timeStr, "unknown", sizeof(timeStr) - 1U);
      timeStr[sizeof(timeStr) - 1U] = '\0';
      LOG_WARNING("ESP32: GetTime failed (status %d)", (int)timeStatus);
    }
    else
    {
      time_t epoch;

      if (ESP32_ParseTime(timeStr, &epoch) == ESP32_OK)
      {
        LogPort_SetEpoch(epoch);
      }
    }
    if (rssiStatus != ESP32_OK)
    {
      rssi = 0;
      LOG_WARNING("ESP32: GetRSSI failed (status %d)", (int)rssiStatus);
    }

    (void)snprintf(payload, sizeof(payload), "%d", (int)rssi);

    if (ESP32_MQTT_Publish(&hEspAt, MQTT_TOPIC, payload, 0U, 0U, 5000U) == ESP32_OK)
    {
      LOG_INFO("ESP32: published to %s: %s", MQTT_TOPIC, payload);
    }
    else
    {
      LOG_ERROR("ESP32: MQTT publish failed");
    }

    WDG_Refresh();
    WDG_DelayMs(APP_ESP32_PUBLISH_MS);
  }
}

UINT App_ESP32_Init(VOID *memory_ptr)
{
  TX_BYTE_POOL *bytePool = (TX_BYTE_POOL *)memory_ptr;
  CHAR *stackPtr = TX_NULL;

  if (tx_byte_allocate(bytePool, (VOID **)&stackPtr, APP_ESP32_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  return tx_thread_create(&appEsp32Thread, "App ESP32", AppEsp32_Entry, 0U,
                           stackPtr, APP_ESP32_STACK_SIZE,
                           APP_ESP32_PRIORITY, APP_ESP32_PRIORITY,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
}
