#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MODBUS_RTU_MAX_ADU        256U
#define MODBUS_MAX_READ_REGS      125U

typedef enum {
  MODBUS_OK                   = 0x00,
  MODBUS_ERR_BUSY             = 0x01, 
  MODBUS_ERR_TX               = 0x02,
  MODBUS_ERR_TIMEOUT          = 0x03, 
  MODBUS_ERR_CRC              = 0x04, 
  MODBUS_ERR_INVALID_RESPONSE = 0x05,
  MODBUS_ERR_EXCEPTION        = 0x06,
  MODBUS_ERR_PARAM            = 0x07,
} Modbus_StatusTypeDef;

typedef struct {
  UART_HandleTypeDef *huart;
  uint32_t            responseTimeout;             
  uint8_t              lastExceptionCode;          
  uint8_t              rxBuf[MODBUS_RTU_MAX_ADU];
  uint8_t              txBuf[MODBUS_RTU_MAX_ADU];
  volatile uint16_t    rxLen;
  volatile uint8_t     rxDone;
} Modbus_HandleTypeDef;

void Modbus_Init(Modbus_HandleTypeDef *hmb, UART_HandleTypeDef *huart, uint32_t responseTimeoutMs);

Modbus_StatusTypeDef Modbus_ReadHoldingRegisters(Modbus_HandleTypeDef *hmb, uint8_t slaveAddr,
                                                  uint16_t startAddr, uint16_t quantity,
                                                  uint16_t *outRegs);

Modbus_StatusTypeDef Modbus_ReadInputRegisters(Modbus_HandleTypeDef *hmb, uint8_t slaveAddr,
                                                uint16_t startAddr, uint16_t quantity,
                                                uint16_t *outRegs);

void Modbus_RxEventCallback(Modbus_HandleTypeDef *hmb, UART_HandleTypeDef *huart, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_RTU_H */
