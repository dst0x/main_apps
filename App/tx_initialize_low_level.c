/**
  ******************************************************************************
  * @file    tx_initialize_low_level.c
  * @brief   ThreadX Cortex-M4/GNU low-level port for STM32F412RETx.
  *
  *          Called once by _tx_initialize_kernel_enter() (i.e. by
  *          tx_kernel_enter(), before tx_application_define()/any thread
  *          runs). By this point main() has already run HAL_Init() and
  *          SystemClock_Config(), so SystemCoreClock is already 96 MHz.
  *
  *          NOTE: this replaces the generic example at
  *          Middlewares/threadx/ports/cortex_m4/gnu/example_build/
  *          tx_initialize_low_level.S, which targets a different linker
  *          layout (_vectors/__RAM_segment_used_end__ symbols that don't
  *          exist in STM32F412RETX_FLASH.ld) and hardcodes a 6 MHz system
  *          clock -- it must not be compiled into this project.
  ******************************************************************************
  */

#include "tx_api.h"
#include "main.h"

/* Defined by the ThreadX common sources (tx_initialize_high_level.c /
 * tx_thread_initialize.c); we only need to set them here, not declare them
 * for ThreadX's own use. */
extern VOID *_tx_initialize_unused_memory;
extern VOID *_tx_thread_system_stack_ptr;

/* End of .bss / start of the linker's heap+stack reservation, see
 * STM32F412RETX_FLASH.ld's ._user_heap_stack section. Passed to
 * tx_application_define() as `first_unused_memory` -- unused by this
 * project's tx_application_define() (see app_azure_rtos.c, which sizes its
 * pools from static arrays instead), but ThreadX's kernel-entry contract
 * still expects a valid pointer here. */
extern uint32_t _end;

void _tx_initialize_low_level(void)
{
  _tx_initialize_unused_memory = (VOID *)&_end;
  _tx_thread_system_stack_ptr  = (VOID *)__get_MSP();

  /* Re-tick SysTick for ThreadX's default 100 Hz (TX_TIMER_TICKS_PER_SECOND)
   * instead of HAL's default 1 kHz. HAL_SetTickFreq() keeps HAL_GetTick()/
   * HAL_Delay() -- and this project's Modbus_ReadHoldingRegisters() response
   * timeout, which is HAL_GetTick()-based -- reporting correct real-world
   * milliseconds despite the slower SysTick period (see stm32f4xx_it.c's
   * SysTick_Handler, which now drives both HAL_IncTick() and
   * _tx_timer_interrupt() from the same 10 ms tick). */
  HAL_SetTickFreq(HAL_TICK_FREQ_100HZ);
  HAL_InitTick(TICK_INT_PRIORITY);

  /* PendSV must be the single lowest-priority interrupt so a context switch
   * never preempts a real ISR; SysTick is left at TICK_INT_PRIORITY (also the
   * lowest, set by HAL_InitTick() above) so the RTOS tick can't starve
   * peripheral IRQs such as USART1/DMA2 (priority 5, see usart.c). */
  HAL_NVIC_SetPriority(PendSV_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL, 0U);
}
