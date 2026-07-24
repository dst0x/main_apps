/**
  ******************************************************************************
  * @file    esp32c3.h
  * @brief   ESP32-C3 AT-command driver: WiFi connect, time/RSSI query, MQTT
  *          publish. Built on Middlewares/esp_at (generic AT transport).
  *
  *          Call order matches the request this was written for:
  *            ESP32_Init()            -- basic AT handshake
  *            ESP32_ConnectWiFi(...)   -- join a WiFi network
  *            ESP32_GetTime(...)       -- current time (SNTP)
  *            ESP32_GetRSSI(...)       -- current AP signal strength
  *            ESP32_MQTT_Connect(...)  -- connect to a broker
  *            ESP32_MQTT_SubscribeAndGetInterval(...)
  *                                     -- (optional) read a pushed config value
  *            ESP32_MQTT_Publish(...)  -- publish one message
  ******************************************************************************
  */

#ifndef __ESP32C3_H
#define __ESP32C3_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_at.h"
#include <stdint.h>
#include <time.h>

typedef enum
{
  ESP32_OK = 0,
  ESP32_ERR_TIMEOUT,
  ESP32_ERR_REPLY,   /* module answered ERROR/FAIL */
  ESP32_ERR_PARAM,
  ESP32_ERR_PARSE,   /* reply didn't contain the field we were looking for */
} ESP32_StatusTypeDef;

/** Basic "is it alive" handshake (AT, ATE0, CWMODE=1). Call once. */
ESP32_StatusTypeDef ESP32_Init(ESPAT_HandleTypeDef *hat);

/** Sends a burst of filler bytes to push the module past any raw-data byte
 *  count it might still be mid-way through counting down (e.g. a
 *  AT+MQTTPUBRAW payload) after an MCU-only reset (IWDG/brownout/...): the
 *  ESP32-C3 does NOT power-cycle when only the STM32 resets, so if the STM32
 *  died mid-transmission the module can be left waiting for bytes that will
 *  now never arrive, and would otherwise silently swallow every subsequent
 *  "AT" as leftover payload instead of parsing it. Call once before the
 *  first AT handshake attempt after boot. */
void ESP32_UartResync(ESPAT_HandleTypeDef *hat);

/** Joins a WiFi network. timeoutMs should be generous (10-30 s typical). */
ESP32_StatusTypeDef ESP32_ConnectWiFi(ESPAT_HandleTypeDef *hat, const char *ssid,
                                       const char *password, uint32_t timeoutMs);

/** Current time via SNTP, as the module's raw AT+CIPSNTPTIME? string. */
ESP32_StatusTypeDef ESP32_GetTime(ESPAT_HandleTypeDef *hat, char *outTimeStr, uint16_t outSize);

/** Parses an ESP32_GetTime() string (asctime-style "Www Mmm d hh:mm:ss yyyy",
 *  as returned by AT+CIPSNTPTIME?) into a time_t. No timezone conversion is
 *  applied -- the fields are taken as-is (SNTP is configured GMT+7 in
 *  ESP32_Init(), so this yields local time directly). */
ESP32_StatusTypeDef ESP32_ParseTime(const char *timeStr, time_t *outEpoch);

/** Current AP's RSSI in dBm (negative), from AT+CWJAP?. */
ESP32_StatusTypeDef ESP32_GetRSSI(ESPAT_HandleTypeDef *hat, int16_t *outRssiDbm);

/** Connects to an MQTT broker (plain TCP, no TLS) using a username/password
 *  login. Pass "" / "" for username/password if the broker allows anonymous
 *  connections. LinkID 0 is used throughout. */
ESP32_StatusTypeDef ESP32_MQTT_Connect(ESPAT_HandleTypeDef *hat, const char *clientId,
                                        const char *username, const char *password,
                                        const char *host, uint16_t port, uint32_t timeoutMs);

/**
  * @brief  Subscribes to a topic (LinkID 0) on the already-connected MQTT
  *         session and waits for a retained {"interval":<seconds>} push,
  *         returning the value via *outSeconds. Checks the subscribe
  *         command's own response first -- a broker typically pushes a
  *         retained message immediately on subscribe, often within the same
  *         response window as the SUBACK's OK -- before falling back to
  *         waiting up to pushTimeoutMs for it to arrive separately.
  * @param  subTimeoutMs  timeout for the AT+MQTTSUB command itself.
  * @param  pushTimeoutMs extra time to wait for the retained push if it
  *                       didn't arrive alongside the subscribe's own OK.
  * @retval ESP32_ERR_TIMEOUT if nothing arrives in time, ESP32_ERR_PARSE if
  *         a push arrived but didn't contain a parseable "interval" field.
  */
ESP32_StatusTypeDef ESP32_MQTT_SubscribeAndGetInterval(ESPAT_HandleTypeDef *hat, const char *topic,
                                                        uint8_t qos, uint32_t subTimeoutMs,
                                                        uint32_t pushTimeoutMs, int32_t *outSeconds);

/** Publishes one message. qos: 0/1/2, retain: 0/1. */
ESP32_StatusTypeDef ESP32_MQTT_Publish(ESPAT_HandleTypeDef *hat, const char *topic,
                                        const char *payload, uint8_t qos, uint8_t retain,
                                        uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32C3_H */
