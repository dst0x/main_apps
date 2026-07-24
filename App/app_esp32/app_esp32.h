/**
  ******************************************************************************
  * @file    app_esp32.h
  * @brief   App module: ESP32-C3 AT-command sequence (WiFi -> time/RSSI ->
  *          MQTT publish), with IWDG protection against a hung AT exchange.
  ******************************************************************************
  */

#ifndef APP_ESP32_H
#define APP_ESP32_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include "esp_at.h"

extern ESPAT_HandleTypeDef hEspAt;

UINT App_ESP32_Init(VOID *memory_ptr);

#ifdef __cplusplus
}
#endif

#endif /* APP_ESP32_H */
