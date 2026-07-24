/**
  ******************************************************************************
  * @file      startup_stm32f412rx.s
  * @brief     Reset handler + vector table for STM32F412RETx (GNU assembler)
  *            Vector order matches IRQn_Type in
  *            Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f412rx.h exactly.
  ******************************************************************************
  */

  .syntax unified
  .cpu cortex-m4
  .fpu fpv4-sp-d16
  .thumb

.global  g_pfnVectors
.global  Default_Handler

/* start address for the initialization values of the .data section, defined in the linker script */
.word  _sidata
/* start address for the .data section, defined in the linker script */
.word  _sdata
/* end address for the .data section, defined in the linker script */
.word  _edata
/* start address for the .bss section, defined in the linker script */
.word  _sbss
/* end address for the .bss section, defined in the linker script */
.word  _ebss

  .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack

  /* Copy the data segment initializers from flash to SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

  /* Zero fill the bss segment */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  bl  SystemInit
  bl  __libc_init_array
  bl  main
  bx  lr
  .size  Reset_Handler, .-Reset_Handler

  .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b  Infinite_Loop
  .size  Default_Handler, .-Default_Handler

/******************************************************************************
* Vector table (must land in the .isr_vector section, see the linker script)
******************************************************************************/
  .section  .isr_vector,"a",%progbits
  .type  g_pfnVectors, %object
  .size  g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler

  /* External Interrupts, order == IRQn_Type in stm32f412rx.h */
  .word  WWDG_IRQHandler                 /* 0   */
  .word  PVD_IRQHandler                  /* 1   */
  .word  TAMP_STAMP_IRQHandler           /* 2   */
  .word  RTC_WKUP_IRQHandler             /* 3   */
  .word  FLASH_IRQHandler                /* 4   */
  .word  RCC_IRQHandler                  /* 5   */
  .word  EXTI0_IRQHandler                /* 6   */
  .word  EXTI1_IRQHandler                /* 7   */
  .word  EXTI2_IRQHandler                /* 8   */
  .word  EXTI3_IRQHandler                /* 9   */
  .word  EXTI4_IRQHandler                /* 10  */
  .word  DMA1_Stream0_IRQHandler         /* 11  */
  .word  DMA1_Stream1_IRQHandler         /* 12  */
  .word  DMA1_Stream2_IRQHandler         /* 13  */
  .word  DMA1_Stream3_IRQHandler         /* 14  */
  .word  DMA1_Stream4_IRQHandler         /* 15  */
  .word  DMA1_Stream5_IRQHandler         /* 16  */
  .word  DMA1_Stream6_IRQHandler         /* 17  */
  .word  ADC_IRQHandler                  /* 18  */
  .word  CAN1_TX_IRQHandler              /* 19  */
  .word  CAN1_RX0_IRQHandler             /* 20  */
  .word  CAN1_RX1_IRQHandler             /* 21  */
  .word  CAN1_SCE_IRQHandler             /* 22  */
  .word  EXTI9_5_IRQHandler              /* 23  */
  .word  TIM1_BRK_TIM9_IRQHandler        /* 24  */
  .word  TIM1_UP_TIM10_IRQHandler        /* 25  */
  .word  TIM1_TRG_COM_TIM11_IRQHandler   /* 26  */
  .word  TIM1_CC_IRQHandler              /* 27  */
  .word  TIM2_IRQHandler                 /* 28  */
  .word  TIM3_IRQHandler                 /* 29  */
  .word  TIM4_IRQHandler                 /* 30  */
  .word  I2C1_EV_IRQHandler              /* 31  */
  .word  I2C1_ER_IRQHandler              /* 32  */
  .word  I2C2_EV_IRQHandler              /* 33  */
  .word  I2C2_ER_IRQHandler              /* 34  */
  .word  SPI1_IRQHandler                 /* 35  */
  .word  SPI2_IRQHandler                 /* 36  */
  .word  USART1_IRQHandler               /* 37  */
  .word  USART2_IRQHandler               /* 38  */
  .word  USART3_IRQHandler               /* 39  */
  .word  EXTI15_10_IRQHandler            /* 40  */
  .word  RTC_Alarm_IRQHandler            /* 41  */
  .word  OTG_FS_WKUP_IRQHandler          /* 42  */
  .word  TIM8_BRK_TIM12_IRQHandler       /* 43  */
  .word  TIM8_UP_TIM13_IRQHandler        /* 44  */
  .word  TIM8_TRG_COM_TIM14_IRQHandler   /* 45  */
  .word  TIM8_CC_IRQHandler              /* 46  */
  .word  DMA1_Stream7_IRQHandler         /* 47  */
  .word  0                               /* 48  - reserved */
  .word  SDIO_IRQHandler                 /* 49  */
  .word  TIM5_IRQHandler                 /* 50  */
  .word  SPI3_IRQHandler                 /* 51  */
  .word  0                               /* 52  - reserved */
  .word  0                               /* 53  - reserved */
  .word  TIM6_IRQHandler                 /* 54  */
  .word  TIM7_IRQHandler                 /* 55  */
  .word  DMA2_Stream0_IRQHandler         /* 56  */
  .word  DMA2_Stream1_IRQHandler         /* 57  */
  .word  DMA2_Stream2_IRQHandler         /* 58  */
  .word  DMA2_Stream3_IRQHandler         /* 59  */
  .word  DMA2_Stream4_IRQHandler         /* 60  */
  .word  DFSDM1_FLT0_IRQHandler          /* 61  */
  .word  DFSDM1_FLT1_IRQHandler          /* 62  */
  .word  CAN2_TX_IRQHandler              /* 63  */
  .word  CAN2_RX0_IRQHandler             /* 64  */
  .word  CAN2_RX1_IRQHandler             /* 65  */
  .word  CAN2_SCE_IRQHandler             /* 66  */
  .word  OTG_FS_IRQHandler               /* 67  */
  .word  DMA2_Stream5_IRQHandler         /* 68  */
  .word  DMA2_Stream6_IRQHandler         /* 69  */
  .word  DMA2_Stream7_IRQHandler         /* 70  */
  .word  USART6_IRQHandler               /* 71  */
  .word  I2C3_EV_IRQHandler              /* 72  */
  .word  I2C3_ER_IRQHandler              /* 73  */
  .word  0                               /* 74  - reserved */
  .word  0                               /* 75  - reserved */
  .word  0                               /* 76  - reserved */
  .word  0                               /* 77  - reserved */
  .word  0                               /* 78  - reserved */
  .word  0                               /* 79  - reserved */
  .word  RNG_IRQHandler                  /* 80  */
  .word  FPU_IRQHandler                  /* 81  */
  .word  0                               /* 82  - reserved */
  .word  0                               /* 83  - reserved */
  .word  SPI4_IRQHandler                 /* 84  */
  .word  SPI5_IRQHandler                 /* 85  */
  .word  0                               /* 86  - reserved */
  .word  0                               /* 87  - reserved */
  .word  0                               /* 88  - reserved */
  .word  0                               /* 89  - reserved */
  .word  0                               /* 90  - reserved */
  .word  0                               /* 91  - reserved */
  .word  QUADSPI_IRQHandler              /* 92  */
  .word  0                               /* 93  - reserved */
  .word  0                               /* 94  - reserved */
  .word  FMPI2C1_EV_IRQHandler           /* 95  */
  .word  FMPI2C1_ER_IRQHandler           /* 96  */

/*******************************************************************************
* Weak aliases: a strong definition anywhere else in the project (stm32f4xx_it.c,
* usart.c's IRQ handlers, or ThreadX's own PendSV_Handler in tx_thread_schedule.S)
* silently overrides the corresponding line here at link time.
*******************************************************************************/
  .weak  NMI_Handler
  .thumb_set  NMI_Handler,Default_Handler

  .weak  HardFault_Handler
  .thumb_set  HardFault_Handler,Default_Handler

  .weak  MemManage_Handler
  .thumb_set  MemManage_Handler,Default_Handler

  .weak  BusFault_Handler
  .thumb_set  BusFault_Handler,Default_Handler

  .weak  UsageFault_Handler
  .thumb_set  UsageFault_Handler,Default_Handler

  .weak  SVC_Handler
  .thumb_set  SVC_Handler,Default_Handler

  .weak  DebugMon_Handler
  .thumb_set  DebugMon_Handler,Default_Handler

  .weak  PendSV_Handler
  .thumb_set  PendSV_Handler,Default_Handler

  .weak  SysTick_Handler
  .thumb_set  SysTick_Handler,Default_Handler

  .weak  WWDG_IRQHandler
  .thumb_set  WWDG_IRQHandler,Default_Handler

  .weak  PVD_IRQHandler
  .thumb_set  PVD_IRQHandler,Default_Handler

  .weak  TAMP_STAMP_IRQHandler
  .thumb_set  TAMP_STAMP_IRQHandler,Default_Handler

  .weak  RTC_WKUP_IRQHandler
  .thumb_set  RTC_WKUP_IRQHandler,Default_Handler

  .weak  FLASH_IRQHandler
  .thumb_set  FLASH_IRQHandler,Default_Handler

  .weak  RCC_IRQHandler
  .thumb_set  RCC_IRQHandler,Default_Handler

  .weak  EXTI0_IRQHandler
  .thumb_set  EXTI0_IRQHandler,Default_Handler

  .weak  EXTI1_IRQHandler
  .thumb_set  EXTI1_IRQHandler,Default_Handler

  .weak  EXTI2_IRQHandler
  .thumb_set  EXTI2_IRQHandler,Default_Handler

  .weak  EXTI3_IRQHandler
  .thumb_set  EXTI3_IRQHandler,Default_Handler

  .weak  EXTI4_IRQHandler
  .thumb_set  EXTI4_IRQHandler,Default_Handler

  .weak  DMA1_Stream0_IRQHandler
  .thumb_set  DMA1_Stream0_IRQHandler,Default_Handler

  .weak  DMA1_Stream1_IRQHandler
  .thumb_set  DMA1_Stream1_IRQHandler,Default_Handler

  .weak  DMA1_Stream2_IRQHandler
  .thumb_set  DMA1_Stream2_IRQHandler,Default_Handler

  .weak  DMA1_Stream3_IRQHandler
  .thumb_set  DMA1_Stream3_IRQHandler,Default_Handler

  .weak  DMA1_Stream4_IRQHandler
  .thumb_set  DMA1_Stream4_IRQHandler,Default_Handler

  .weak  DMA1_Stream5_IRQHandler
  .thumb_set  DMA1_Stream5_IRQHandler,Default_Handler

  .weak  DMA1_Stream6_IRQHandler
  .thumb_set  DMA1_Stream6_IRQHandler,Default_Handler

  .weak  ADC_IRQHandler
  .thumb_set  ADC_IRQHandler,Default_Handler

  .weak  CAN1_TX_IRQHandler
  .thumb_set  CAN1_TX_IRQHandler,Default_Handler

  .weak  CAN1_RX0_IRQHandler
  .thumb_set  CAN1_RX0_IRQHandler,Default_Handler

  .weak  CAN1_RX1_IRQHandler
  .thumb_set  CAN1_RX1_IRQHandler,Default_Handler

  .weak  CAN1_SCE_IRQHandler
  .thumb_set  CAN1_SCE_IRQHandler,Default_Handler

  .weak  EXTI9_5_IRQHandler
  .thumb_set  EXTI9_5_IRQHandler,Default_Handler

  .weak  TIM1_BRK_TIM9_IRQHandler
  .thumb_set  TIM1_BRK_TIM9_IRQHandler,Default_Handler

  .weak  TIM1_UP_TIM10_IRQHandler
  .thumb_set  TIM1_UP_TIM10_IRQHandler,Default_Handler

  .weak  TIM1_TRG_COM_TIM11_IRQHandler
  .thumb_set  TIM1_TRG_COM_TIM11_IRQHandler,Default_Handler

  .weak  TIM1_CC_IRQHandler
  .thumb_set  TIM1_CC_IRQHandler,Default_Handler

  .weak  TIM2_IRQHandler
  .thumb_set  TIM2_IRQHandler,Default_Handler

  .weak  TIM3_IRQHandler
  .thumb_set  TIM3_IRQHandler,Default_Handler

  .weak  TIM4_IRQHandler
  .thumb_set  TIM4_IRQHandler,Default_Handler

  .weak  I2C1_EV_IRQHandler
  .thumb_set  I2C1_EV_IRQHandler,Default_Handler

  .weak  I2C1_ER_IRQHandler
  .thumb_set  I2C1_ER_IRQHandler,Default_Handler

  .weak  I2C2_EV_IRQHandler
  .thumb_set  I2C2_EV_IRQHandler,Default_Handler

  .weak  I2C2_ER_IRQHandler
  .thumb_set  I2C2_ER_IRQHandler,Default_Handler

  .weak  SPI1_IRQHandler
  .thumb_set  SPI1_IRQHandler,Default_Handler

  .weak  SPI2_IRQHandler
  .thumb_set  SPI2_IRQHandler,Default_Handler

  .weak  USART1_IRQHandler
  .thumb_set  USART1_IRQHandler,Default_Handler

  .weak  USART2_IRQHandler
  .thumb_set  USART2_IRQHandler,Default_Handler

  .weak  USART3_IRQHandler
  .thumb_set  USART3_IRQHandler,Default_Handler

  .weak  EXTI15_10_IRQHandler
  .thumb_set  EXTI15_10_IRQHandler,Default_Handler

  .weak  RTC_Alarm_IRQHandler
  .thumb_set  RTC_Alarm_IRQHandler,Default_Handler

  .weak  OTG_FS_WKUP_IRQHandler
  .thumb_set  OTG_FS_WKUP_IRQHandler,Default_Handler

  .weak  TIM8_BRK_TIM12_IRQHandler
  .thumb_set  TIM8_BRK_TIM12_IRQHandler,Default_Handler

  .weak  TIM8_UP_TIM13_IRQHandler
  .thumb_set  TIM8_UP_TIM13_IRQHandler,Default_Handler

  .weak  TIM8_TRG_COM_TIM14_IRQHandler
  .thumb_set  TIM8_TRG_COM_TIM14_IRQHandler,Default_Handler

  .weak  TIM8_CC_IRQHandler
  .thumb_set  TIM8_CC_IRQHandler,Default_Handler

  .weak  DMA1_Stream7_IRQHandler
  .thumb_set  DMA1_Stream7_IRQHandler,Default_Handler

  .weak  SDIO_IRQHandler
  .thumb_set  SDIO_IRQHandler,Default_Handler

  .weak  TIM5_IRQHandler
  .thumb_set  TIM5_IRQHandler,Default_Handler

  .weak  SPI3_IRQHandler
  .thumb_set  SPI3_IRQHandler,Default_Handler

  .weak  TIM6_IRQHandler
  .thumb_set  TIM6_IRQHandler,Default_Handler

  .weak  TIM7_IRQHandler
  .thumb_set  TIM7_IRQHandler,Default_Handler

  .weak  DMA2_Stream0_IRQHandler
  .thumb_set  DMA2_Stream0_IRQHandler,Default_Handler

  .weak  DMA2_Stream1_IRQHandler
  .thumb_set  DMA2_Stream1_IRQHandler,Default_Handler

  .weak  DMA2_Stream2_IRQHandler
  .thumb_set  DMA2_Stream2_IRQHandler,Default_Handler

  .weak  DMA2_Stream3_IRQHandler
  .thumb_set  DMA2_Stream3_IRQHandler,Default_Handler

  .weak  DMA2_Stream4_IRQHandler
  .thumb_set  DMA2_Stream4_IRQHandler,Default_Handler

  .weak  DFSDM1_FLT0_IRQHandler
  .thumb_set  DFSDM1_FLT0_IRQHandler,Default_Handler

  .weak  DFSDM1_FLT1_IRQHandler
  .thumb_set  DFSDM1_FLT1_IRQHandler,Default_Handler

  .weak  CAN2_TX_IRQHandler
  .thumb_set  CAN2_TX_IRQHandler,Default_Handler

  .weak  CAN2_RX0_IRQHandler
  .thumb_set  CAN2_RX0_IRQHandler,Default_Handler

  .weak  CAN2_RX1_IRQHandler
  .thumb_set  CAN2_RX1_IRQHandler,Default_Handler

  .weak  CAN2_SCE_IRQHandler
  .thumb_set  CAN2_SCE_IRQHandler,Default_Handler

  .weak  OTG_FS_IRQHandler
  .thumb_set  OTG_FS_IRQHandler,Default_Handler

  .weak  DMA2_Stream5_IRQHandler
  .thumb_set  DMA2_Stream5_IRQHandler,Default_Handler

  .weak  DMA2_Stream6_IRQHandler
  .thumb_set  DMA2_Stream6_IRQHandler,Default_Handler

  .weak  DMA2_Stream7_IRQHandler
  .thumb_set  DMA2_Stream7_IRQHandler,Default_Handler

  .weak  USART6_IRQHandler
  .thumb_set  USART6_IRQHandler,Default_Handler

  .weak  I2C3_EV_IRQHandler
  .thumb_set  I2C3_EV_IRQHandler,Default_Handler

  .weak  I2C3_ER_IRQHandler
  .thumb_set  I2C3_ER_IRQHandler,Default_Handler

  .weak  RNG_IRQHandler
  .thumb_set  RNG_IRQHandler,Default_Handler

  .weak  FPU_IRQHandler
  .thumb_set  FPU_IRQHandler,Default_Handler

  .weak  SPI4_IRQHandler
  .thumb_set  SPI4_IRQHandler,Default_Handler

  .weak  SPI5_IRQHandler
  .thumb_set  SPI5_IRQHandler,Default_Handler

  .weak  QUADSPI_IRQHandler
  .thumb_set  QUADSPI_IRQHandler,Default_Handler

  .weak  FMPI2C1_EV_IRQHandler
  .thumb_set  FMPI2C1_EV_IRQHandler,Default_Handler

  .weak  FMPI2C1_ER_IRQHandler
  .thumb_set  FMPI2C1_ER_IRQHandler,Default_Handler
