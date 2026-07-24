/**
  ******************************************************************************
  * @file    main.h
  * @brief   Header for main.c (STM32F412RETx clock tree bring-up)
  ******************************************************************************
  */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "modbus_rtu.h"

/* Shared Modbus RTU master instance (bound to huart1 in main.c),
 * referenced from stm32f4xx_it.c's HAL_UARTEx_RxEventCallback() */
extern Modbus_HandleTypeDef hmodbusSensor;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
