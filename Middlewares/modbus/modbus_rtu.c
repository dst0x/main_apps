#include "modbus_rtu.h"
#include <string.h>

static uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len)
{
  uint16_t crc = 0xFFFFU;

  for (uint16_t pos = 0; pos < len; pos++)
  {
    crc ^= (uint16_t)buf[pos];
    for (uint8_t i = 0; i < 8U; i++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc >>= 1;
        crc ^= 0xA001U;
      }
      else
      {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void Modbus_Init(Modbus_HandleTypeDef *hmb, UART_HandleTypeDef *huart, uint32_t responseTimeoutMs)
{
  memset(hmb, 0, sizeof(*hmb));
  hmb->huart = huart;
  hmb->responseTimeout = responseTimeoutMs;
}

static Modbus_StatusTypeDef Modbus_ReadRegisters(Modbus_HandleTypeDef *hmb, uint8_t functionCode,
                                                  uint8_t slaveAddr, uint16_t startAddr,
                                                  uint16_t quantity, uint16_t *outRegs)
{
  uint16_t crc;
  uint32_t start;
  const uint8_t *rx;
  uint16_t rxLen;
  uint16_t rxCrc;
  uint16_t recvCrc;
  uint8_t byteCount;

  if ((hmb == NULL) || (hmb->huart == NULL) || (outRegs == NULL))
  {
    return MODBUS_ERR_PARAM;
  }
  if ((quantity == 0U) || (quantity > MODBUS_MAX_READ_REGS))
  {
    return MODBUS_ERR_PARAM;
  }

  hmb->txBuf[0] = slaveAddr;
  hmb->txBuf[1] = functionCode;
  hmb->txBuf[2] = (uint8_t)(startAddr >> 8);
  hmb->txBuf[3] = (uint8_t)(startAddr & 0xFFU);
  hmb->txBuf[4] = (uint8_t)(quantity >> 8);
  hmb->txBuf[5] = (uint8_t)(quantity & 0xFFU);
  crc = Modbus_CRC16(hmb->txBuf, 6U);
  hmb->txBuf[6] = (uint8_t)(crc & 0xFFU); 
  hmb->txBuf[7] = (uint8_t)(crc >> 8);

  (void)HAL_UART_Abort(hmb->huart);

  hmb->rxDone = 0U;
  hmb->rxLen = 0U;

  if (HAL_UARTEx_ReceiveToIdle_DMA(hmb->huart, hmb->rxBuf, MODBUS_RTU_MAX_ADU) != HAL_OK)
  {
    return MODBUS_ERR_TX;
  }

  if (HAL_UART_Transmit_DMA(hmb->huart, hmb->txBuf, 8U) != HAL_OK)
  {
    (void)HAL_UART_Abort(hmb->huart);
    return MODBUS_ERR_TX;
  }

  start = HAL_GetTick();
  while (hmb->rxDone == 0U)
  {
    if ((HAL_GetTick() - start) > hmb->responseTimeout)
    {
      (void)HAL_UART_Abort(hmb->huart);
      return MODBUS_ERR_TIMEOUT;
    }
  }

  rx = hmb->rxBuf;
  rxLen = hmb->rxLen;

  if (rxLen < 5U)
  {
    return MODBUS_ERR_INVALID_RESPONSE;
  }

  rxCrc = Modbus_CRC16(rx, (uint16_t)(rxLen - 2U));
  recvCrc = (uint16_t)rx[rxLen - 2U] | ((uint16_t)rx[rxLen - 1U] << 8);
  if (rxCrc != recvCrc)
  {
    return MODBUS_ERR_CRC;
  }

  if (rx[0] != slaveAddr)
  {
    return MODBUS_ERR_INVALID_RESPONSE;
  }

  if (rx[1] == (uint8_t)(functionCode | 0x80U))
  {
    hmb->lastExceptionCode = rx[2];
    return MODBUS_ERR_EXCEPTION;
  }

  if (rx[1] != functionCode)
  {
    return MODBUS_ERR_INVALID_RESPONSE;
  }

  byteCount = rx[2];
  if ((byteCount != (uint8_t)(quantity * 2U)) || (rxLen != (uint16_t)(3U + byteCount + 2U)))
  {
    return MODBUS_ERR_INVALID_RESPONSE;
  }

  for (uint16_t i = 0; i < quantity; i++)
  {
    outRegs[i] = ((uint16_t)rx[3U + (2U * i)] << 8) | (uint16_t)rx[4U + (2U * i)];
  }

  return MODBUS_OK;
}

Modbus_StatusTypeDef Modbus_ReadHoldingRegisters(Modbus_HandleTypeDef *hmb, uint8_t slaveAddr,
                                                  uint16_t startAddr, uint16_t quantity,
                                                  uint16_t *outRegs)
{
  return Modbus_ReadRegisters(hmb, 0x03U, slaveAddr, startAddr, quantity, outRegs);
}

Modbus_StatusTypeDef Modbus_ReadInputRegisters(Modbus_HandleTypeDef *hmb, uint8_t slaveAddr,
                                                uint16_t startAddr, uint16_t quantity,
                                                uint16_t *outRegs)
{
  return Modbus_ReadRegisters(hmb, 0x04U, slaveAddr, startAddr, quantity, outRegs);
}

void Modbus_RxEventCallback(Modbus_HandleTypeDef *hmb, UART_HandleTypeDef *huart, uint16_t Size)
{
  if ((hmb != NULL) && (hmb->huart == huart))
  {
    hmb->rxLen = Size;
    hmb->rxDone = 1U;
  }
}
