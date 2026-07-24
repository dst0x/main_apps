#include "esp_at.h"
#include <string.h>
#include <stdio.h>

void ESPAT_Init(ESPAT_HandleTypeDef *hat, UART_HandleTypeDef *huart, void (*onWaitTick)(void))
{
  memset(hat, 0, sizeof(*hat));
  hat->huart      = huart;
  hat->onWaitTick = onWaitTick;
}

static void ESPAT_Arm(ESPAT_HandleTypeDef *hat)
{
  hat->lastArmStatus = HAL_UARTEx_ReceiveToIdle_IT(hat->huart, hat->chunkBuf, ESPAT_CHUNK_BUF_SIZE);
  hat->rxArmed = (hat->lastArmStatus == HAL_OK) ? 1U : 0U;
}

static ESPAT_StatusTypeDef ESPAT_WaitReply(ESPAT_HandleTypeDef *hat, uint32_t timeoutMs,
                                            char *outResp, uint16_t outRespSize)
{
  uint32_t start = HAL_GetTick();

  for (;;)
  {
    if (hat->rxDone != 0U)
    {
      uint16_t copyLen = hat->rxLen;

      hat->rxDone  = 0U;
      hat->rxArmed = 0U;

      /* Append this chunk to the accumulated response (truncate, don't overflow). */
      if ((uint32_t)(hat->respLen + copyLen) >= sizeof(hat->respBuf))
      {
        copyLen = (uint16_t)(sizeof(hat->respBuf) - 1U - hat->respLen);
      }
      memcpy(&hat->respBuf[hat->respLen], hat->chunkBuf, copyLen);
      hat->respLen += copyLen;
      hat->respBuf[hat->respLen] = '\0';

      if ((strstr(hat->respBuf, "ERROR") != NULL) || (strstr(hat->respBuf, "FAIL") != NULL))
      {
        (void)HAL_UART_Abort(hat->huart);
        if ((outResp != NULL) && (outRespSize > 0U))
        {
          strncpy(outResp, hat->respBuf, outRespSize - 1U);
          outResp[outRespSize - 1U] = '\0';
        }
        return ESPAT_ERR_REPLY;
      }

      if ((strstr(hat->respBuf, "OK\r\n") != NULL) || (strstr(hat->respBuf, "OK\r\n>") != NULL)
          || (strchr(hat->respBuf, '>') != NULL))
      {
        (void)HAL_UART_Abort(hat->huart);
        if ((outResp != NULL) && (outRespSize > 0U))
        {
          strncpy(outResp, hat->respBuf, outRespSize - 1U);
          outResp[outRespSize - 1U] = '\0';
        }
        return ESPAT_OK;
      }

      if (hat->respLen >= (sizeof(hat->respBuf) - 1U))
      {
        (void)HAL_UART_Abort(hat->huart);
        return ESPAT_ERR_OVERFLOW;
      }

      /* Not done yet (more URC lines may follow) -- keep listening. */
      ESPAT_Arm(hat);
    }

    if ((HAL_GetTick() - start) > timeoutMs)
    {
      (void)HAL_UART_Abort(hat->huart);
      if ((outResp != NULL) && (outRespSize > 0U))
      {
        strncpy(outResp, hat->respBuf, outRespSize - 1U);
        outResp[outRespSize - 1U] = '\0';
      }
      return ESPAT_ERR_TIMEOUT;
    }

    if (hat->onWaitTick != NULL)
    {
      hat->onWaitTick();
    }
  }
}

ESPAT_StatusTypeDef ESPAT_SendCommand(ESPAT_HandleTypeDef *hat, const char *cmd,
                                       uint32_t timeoutMs, char *outResp, uint16_t outRespSize)
{
  char txLine[ESPAT_CMD_BUF_SIZE];
  int  txLen;

  if ((hat == NULL) || (hat->huart == NULL) || (cmd == NULL))
  {
    return ESPAT_ERR_PARAM;
  }

  txLen = snprintf(txLine, sizeof(txLine), "%s\r\n", cmd);
  if ((txLen <= 0) || ((uint32_t)txLen >= sizeof(txLine)))
  {
    return ESPAT_ERR_PARAM;
  }

  (void)HAL_UART_Abort(hat->huart);

  hat->respLen = 0U;
  hat->respBuf[0] = '\0';
  hat->rxDone  = 0U;
  hat->rxArmed = 0U;

  /* Arm the receiver before transmitting so a fast reply is never missed. */
  ESPAT_Arm(hat);

  hat->lastTxStatus = HAL_UART_Transmit(hat->huart, (uint8_t *)txLine, (uint16_t)txLen, 2000U);
  if (hat->lastTxStatus != HAL_OK)
  {
    (void)HAL_UART_Abort(hat->huart);
    return ESPAT_ERR_TX;
  }

  return ESPAT_WaitReply(hat, timeoutMs, outResp, outRespSize);
}

ESPAT_StatusTypeDef ESPAT_SendRaw(ESPAT_HandleTypeDef *hat, const uint8_t *data, uint16_t dataLen,
                                   uint32_t timeoutMs, char *outResp, uint16_t outRespSize)
{
  if ((hat == NULL) || (hat->huart == NULL) || (data == NULL))
  {
    return ESPAT_ERR_PARAM;
  }

  (void)HAL_UART_Abort(hat->huart);

  hat->respLen = 0U;
  hat->respBuf[0] = '\0';
  hat->rxDone  = 0U;
  hat->rxArmed = 0U;

  /* Arm the receiver before transmitting so a fast reply is never missed. */
  ESPAT_Arm(hat);

  hat->lastTxStatus = HAL_UART_Transmit(hat->huart, (uint8_t *)data, dataLen, 2000U);
  if (hat->lastTxStatus != HAL_OK)
  {
    (void)HAL_UART_Abort(hat->huart);
    return ESPAT_ERR_TX;
  }

  return ESPAT_WaitReply(hat, timeoutMs, outResp, outRespSize);
}

ESPAT_StatusTypeDef ESPAT_WaitUnsolicited(ESPAT_HandleTypeDef *hat, const char *needle,
                                           uint32_t timeoutMs, char *outResp, uint16_t outRespSize)
{
  uint32_t start;

  if ((hat == NULL) || (hat->huart == NULL) || (needle == NULL))
  {
    return ESPAT_ERR_PARAM;
  }

  hat->respLen = 0U;
  hat->respBuf[0] = '\0';
  hat->rxDone = 0U;

  if (hat->rxArmed == 0U)
  {
    ESPAT_Arm(hat);
  }

  start = HAL_GetTick();

  for (;;)
  {
    if (hat->rxDone != 0U)
    {
      uint16_t copyLen = hat->rxLen;

      hat->rxDone  = 0U;
      hat->rxArmed = 0U;

      if ((uint32_t)(hat->respLen + copyLen) >= sizeof(hat->respBuf))
      {
        copyLen = (uint16_t)(sizeof(hat->respBuf) - 1U - hat->respLen);
      }
      memcpy(&hat->respBuf[hat->respLen], hat->chunkBuf, copyLen);
      hat->respLen += copyLen;
      hat->respBuf[hat->respLen] = '\0';

      if (strstr(hat->respBuf, needle) != NULL)
      {
        (void)HAL_UART_Abort(hat->huart);
        if ((outResp != NULL) && (outRespSize > 0U))
        {
          strncpy(outResp, hat->respBuf, outRespSize - 1U);
          outResp[outRespSize - 1U] = '\0';
        }
        return ESPAT_OK;
      }

      if (hat->respLen >= (sizeof(hat->respBuf) - 1U))
      {
        (void)HAL_UART_Abort(hat->huart);
        return ESPAT_ERR_OVERFLOW;
      }

      ESPAT_Arm(hat);
    }

    if ((HAL_GetTick() - start) > timeoutMs)
    {
      (void)HAL_UART_Abort(hat->huart);
      return ESPAT_ERR_TIMEOUT;
    }

    if (hat->onWaitTick != NULL)
    {
      hat->onWaitTick();
    }
  }
}

void ESPAT_RxEventCallback(ESPAT_HandleTypeDef *hat, UART_HandleTypeDef *huart, uint16_t Size)
{
  if ((hat != NULL) && (hat->huart == huart))
  {
    hat->rxLen  = Size;
    hat->rxDone = 1U;
  }
}
