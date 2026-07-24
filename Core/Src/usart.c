/**
  ******************************************************************************
  * @file    usart.c
  * @brief   USART1 init: 9600 8N1, RS-485 (auto-direction transceiver, no DE pin)
  *          PA9 = TX, PA10 = RX (AF7)
  ******************************************************************************
  */

#include "usart.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;
DMA_HandleTypeDef  hdma_usart1_tx;

UART_HandleTypeDef huart2;

UART_HandleTypeDef huart6;

void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 9600;   /* match the sensor's Modbus RTU baud rate */
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Debug log console: 115200 8N1, blocking TX. Used by log_port.c's
  *         _write() retarget so LOG_INFO/LOG_WARNING/LOG_ERROR (Middlewares/log)
  *         come out on PA2 (USART2_TX).
  */
void MX_USART2_UART_Init(void)
{
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 115200;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  ESP32-C3 AT command transport: 115200 8N1, interrupt-only (no
  *         DMA needed -- AT command traffic is low-rate). PC6 = TX, PC7 = RX.
  */
void MX_USART6_UART_Init(void)
{
  huart6.Instance          = USART6;
  huart6.Init.BaudRate     = 115200;
  huart6.Init.WordLength   = UART_WORDLENGTH_8B;
  huart6.Init.StopBits     = UART_STOPBITS_1;
  huart6.Init.Parity       = UART_PARITY_NONE;
  huart6.Init.Mode         = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  UART MSP init -- called by HAL_UART_Init(). Enables clocks, configures
  *         PA9/PA10 as USART1 AF, sets up DMA2 Stream2 (RX) / Stream7 (TX) and
  *         the NVIC lines for USART1's IDLE interrupt and both DMA streams.
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == USART6)
  {
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC6 = USART6_TX, PC7 = USART6_RX */
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    return;
  }

  if (huart->Instance == USART2)
  {
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2 = USART2_TX, PA3 = USART2_RX */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    return;
  }

  if (huart->Instance != USART1)
  {
    return;
  }

  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* PA9 = USART1_TX, PA10 = USART1_RX */
  GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* DMA2 Stream2 Channel4 = USART1_RX */
  hdma_usart1_rx.Instance                 = DMA2_Stream2;
  hdma_usart1_rx.Init.Channel             = DMA_CHANNEL_4;
  hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_usart1_rx.Init.Mode                = DMA_NORMAL;
  hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_HIGH;
  hdma_usart1_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

  /* DMA2 Stream7 Channel4 = USART1_TX */
  hdma_usart1_tx.Instance                 = DMA2_Stream7;
  hdma_usart1_tx.Init.Channel             = DMA_CHANNEL_4;
  hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
  hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
  hdma_usart1_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);

  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

  /* USART1 IRQ is required even in DMA mode: it carries the IDLE-line event
   * that HAL_UARTEx_ReceiveToIdle_DMA() uses to close a variable-length frame */
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART6)
  {
    __HAL_RCC_USART6_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
    HAL_NVIC_DisableIRQ(USART6_IRQn);
    return;
  }

  if (huart->Instance == USART2)
  {
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
    return;
  }

  if (huart->Instance != USART1)
  {
    return;
  }

  __HAL_RCC_USART1_CLK_DISABLE();
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

  HAL_DMA_DeInit(huart->hdmarx);
  HAL_DMA_DeInit(huart->hdmatx);

  HAL_NVIC_DisableIRQ(DMA2_Stream2_IRQn);
  HAL_NVIC_DisableIRQ(DMA2_Stream7_IRQn);
  HAL_NVIC_DisableIRQ(USART1_IRQn);
}
