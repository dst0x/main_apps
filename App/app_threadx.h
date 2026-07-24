/**
  ******************************************************************************
  * @file    app_threadx.h
  * @brief   ThreadX application header -- BGT-W87x Modbus sensor thread
  ******************************************************************************
  */

#ifndef APP_THREADX_H
#define APP_THREADX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"
#include <stdint.h>

/**
  * @brief  0 until ThreadX's timer subsystem (_tx_timer_current_ptr etc., set
  *         up inside _tx_initialize_high_level(), called from tx_kernel_enter())
  *         is actually ready. SysTick_Handler() (stm32f4xx_it.c) must not call
  *         _tx_timer_interrupt() while this is 0 -- HAL_Init() in main()
  *         starts SysTick ticking long before tx_kernel_enter() runs, and
  *         calling into ThreadX's timer ISR before it's initialized corrupts
  *         its internal state (symptom: works for exactly one tx_thread_sleep()
  *         cycle, then every subsequent sleep hangs forever).
  *         Set to 1 by tx_application_define() (app_azure_rtos.c), which runs
  *         after the timer subsystem is initialized but before any thread does.
  */
extern volatile uint8_t g_ThreadXTickReady;

/**
  * @brief  Called from tx_application_define() (see app_azure_rtos.c) with the
  *         app byte pool -- creates the BGT-W87x sensor reading thread.
  */
UINT App_ThreadX_Init(VOID *memory_ptr);

/**
  * @brief  Call once from main(), after all plain-HAL setup (SystemClock_Config(),
  *         MX_USART1_UART_Init(), Modbus_Init(), ...) is done. Enters the ThreadX
  *         kernel via tx_kernel_enter() -- this call never returns.
  */
void MX_ThreadX_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_THREADX_H */
