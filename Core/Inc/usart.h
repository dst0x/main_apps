/**
  ******************************************************************************
  * @file    usart.h
  * @brief   USART1 (Modbus RTU transport, DMA) + USART2 (debug log console,
  *          blocking, 115200 8N1, PA2=TX/PA3=RX) + USART6 (ESP32-C3 AT
  *          command transport, interrupt-only (no DMA), 115200 8N1,
  *          PC6=TX/PC7=RX)
  ******************************************************************************
  */

#ifndef __USART_H
#define __USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart6;

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART6_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H */
