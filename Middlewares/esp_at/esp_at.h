#ifndef __ESP_AT_H
#define __ESP_AT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define ESPAT_RESP_BUF_SIZE   768U  /* accumulated response text, incl. NUL */
#define ESPAT_CHUNK_BUF_SIZE  256U  /* single DMA/IDLE reception burst */
#define ESPAT_CMD_BUF_SIZE    384U  /* outgoing "cmd\r\n" line, e.g. MQTTPUB with a JSON payload */

typedef enum
{
  ESPAT_OK = 0,
  ESPAT_ERR_TIMEOUT,      /* no OK/ERROR/FAIL/'>' before timeoutMs */
  ESPAT_ERR_REPLY,        /* module replied ERROR or FAIL */
  ESPAT_ERR_TX,           /* HAL_UART_Transmit_DMA failed to start */
  ESPAT_ERR_OVERFLOW,     /* response longer than ESPAT_RESP_BUF_SIZE */
  ESPAT_ERR_PARAM,
} ESPAT_StatusTypeDef;

typedef struct
{
  UART_HandleTypeDef *huart;

  /* Called periodically while waiting for a response (e.g. WDG_Refresh()) --
   * WiFi/MQTT commands can legitimately take 10-20 s, so whatever watchdog
   * scheme the app uses must be pet from inside that wait, not just once per
   * outer loop. May be NULL. */
  void (*onWaitTick)(void);

  /* internal state -- do not touch from application code, EXCEPT
   * lastTxStatus/lastArmStatus, which are exposed read-only for diagnostic
   * logging (see ESP32_Init()'s failure log in esp32c3.c). */
  uint8_t           chunkBuf[ESPAT_CHUNK_BUF_SIZE];
  char              respBuf[ESPAT_RESP_BUF_SIZE];
  uint16_t          respLen;
  volatile uint16_t rxLen;
  volatile uint8_t  rxDone;
  uint8_t           rxArmed;
  HAL_StatusTypeDef lastTxStatus;  /* result of the last HAL_UART_Transmit() */
  HAL_StatusTypeDef lastArmStatus; /* result of the last HAL_UARTEx_ReceiveToIdle_IT() */
} ESPAT_HandleTypeDef;

void ESPAT_Init(ESPAT_HandleTypeDef *hat, UART_HandleTypeDef *huart, void (*onWaitTick)(void));

/**
  * @brief  Sends "%s\r\n" formatted from cmd, then collects the reply.
  * @param  outResp   optional; if non-NULL, receives a NUL-terminated copy of
  *                    everything the module sent back (up to outRespSize-1
  *                    bytes). Pass NULL if you don't need it.
  * @retval ESPAT_OK on a reply ending in OK (or containing '>'),
  *         ESPAT_ERR_REPLY on ERROR/FAIL, ESPAT_ERR_TIMEOUT otherwise.
  */
ESPAT_StatusTypeDef ESPAT_SendCommand(ESPAT_HandleTypeDef *hat, const char *cmd,
                                       uint32_t timeoutMs, char *outResp, uint16_t outRespSize);

/**
  * @brief  Like ESPAT_SendCommand, but transmits `dataLen` raw bytes verbatim
  *         (no "\r\n" appended, no text formatting) and then waits for a
  *         reply the same way. For the raw-payload phase of commands that
  *         prompt with '>' first, e.g. AT+MQTTPUBRAW, AT+CIPSEND.
  */
ESPAT_StatusTypeDef ESPAT_SendRaw(ESPAT_HandleTypeDef *hat, const uint8_t *data, uint16_t dataLen,
                                   uint32_t timeoutMs, char *outResp, uint16_t outRespSize);

/**
  * @brief  Listen-only wait for an unsolicited push (a URC with no command of
  *         ours behind it, e.g. "+MQTTSUBRECV:..." arriving sometime after a
  *         AT+MQTTSUB's own OK was already consumed). Unlike
  *         ESPAT_SendCommand()/ESPAT_SendRaw(), this transmits nothing and
  *         does not treat "OK\r\n"/"ERROR"/"FAIL"/'>' as terminal -- it waits
  *         until `needle` appears in the accumulated response, or timeoutMs
  *         elapses.
  * @retval ESPAT_OK if `needle` was seen, ESPAT_ERR_TIMEOUT otherwise.
  */
ESPAT_StatusTypeDef ESPAT_WaitUnsolicited(ESPAT_HandleTypeDef *hat, const char *needle,
                                           uint32_t timeoutMs, char *outResp, uint16_t outRespSize);

/**
  * @brief  Feed this from HAL_UARTEx_RxEventCallback() in stm32f4xx_it.c for
  *         every ESPAT_HandleTypeDef in use.
  */
void ESPAT_RxEventCallback(ESPAT_HandleTypeDef *hat, UART_HandleTypeDef *huart, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __ESP_AT_H */
