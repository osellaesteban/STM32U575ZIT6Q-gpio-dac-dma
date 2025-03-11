/*
 * st_STM32_HAL.c
 *
 *  Created on: Mar 11, 2025
 *      Author: osell
 */

#include "st_STM32_HAL.h"
#include "linked_list.h"

#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32u5xx_hal_dac.h"
#define NVALS 16

// Private variables.
DAC_HandleTypeDef hdac;

DMA_HandleTypeDef h_GPDMA_GPIO;
DMA_HandleTypeDef h_GPDMA_DAC;
extern DMA_QListTypeDef GPIOQueue;
extern DMA_QListTypeDef DACQueue;
extern uint32_t DACVals[NVALS];
extern uint32_tGPIOEVals[NVALS];



TIM_HandleTypeDef htim;


void st_GPDMA_ConfigLink(){
	MX_GPIOQueue_Config();
	HAL_DMAEx_List_LinkQ(&h_GPDMA_GPIO, &GPIOQueue);
	HAL_DMAEx_List_Start(&h_GPDMA_GPIO);

	MX_DACQueue_Config();
	HAL_DMAEx_List_LinkQ(&h_GPDMA_DAC, &DACQueue);
	__HAL_LINKDMA(&hdac, DMA_Handle1, h_GPDMA_DAC);
}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
void st_GPDMA_Init(void)
{

	/* USER CODE BEGIN GPDMA1_Init 0 */

	/* USER CODE END GPDMA1_Init 0 */

	/* Peripheral clock enable */
	__HAL_RCC_GPDMA1_CLK_ENABLE();

	/* GPDMA1 interrupt Init */
	HAL_NVIC_SetPriority(GPDMA1_Channel10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel10_IRQn);
	HAL_NVIC_SetPriority(GPDMA1_Channel11_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel11_IRQn);

	/* USER CODE BEGIN GPDMA1_Init 1 */

	/* USER CODE END GPDMA1_Init 1 */
	h_GPDMA_GPIO.Instance = GPDMA1_Channel11;
	h_GPDMA_GPIO.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
	h_GPDMA_GPIO.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	h_GPDMA_GPIO.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	h_GPDMA_GPIO.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	h_GPDMA_GPIO.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
	if (HAL_DMAEx_List_Init(&h_GPDMA_GPIO) != HAL_OK)
	{
		st_Error_Handler();
	}
	if (HAL_DMA_ConfigChannelAttributes(&h_GPDMA_GPIO, DMA_CHANNEL_NPRIV) != HAL_OK)
	{
	Error_Handler();
	}
	h_GPDMA_DAC.Instance = GPDMA1_Channel10;
	h_GPDMA_DAC.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
	h_GPDMA_DAC.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	h_GPDMA_DAC.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	h_GPDMA_DAC.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	h_GPDMA_DAC.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
	if (HAL_DMAEx_List_Init(&h_GPDMA_DAC) != HAL_OK)
	{
		st_Error_Handler();
	}
	if (HAL_DMA_ConfigChannelAttributes(&h_GPDMA_DAC, DMA_CHANNEL_NPRIV) != HAL_OK)
	{
		st_Error_Handler();
	}
	/* USER CODE BEGIN GPDMA1_Init 2 */

	/* USER CODE END GPDMA1_Init 2 */

}




/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
void st_TIM_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  uint32_t clkfreq1 = HAL_RCC_GetPCLK1Freq();
  uint32_t clkfreq2 = HAL_RCC_GetPCLK2Freq();

  /* USER CODE END TIM2_Init 1 */
  htim.Instance = TIM2;
  htim.Init.Prescaler = 20-1;
  htim.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim.Init.Period = 1;
  htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim, &sClockSourceConfig) != HAL_OK)
  {
	  st_Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim, &sMasterConfig) != HAL_OK)
  {
	  st_Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}



/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void st_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, PE2_Pin|PE3_Pin|PE4_Pin|PE5_Pin
                          |PE6_Pin|PE0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2_Pin PE3_Pin PE4_Pin PE5_Pin
                           PE6_Pin PE0_Pin */
  GPIO_InitStruct.Pin = PE2_Pin|PE3_Pin|PE4_Pin|PE5_Pin
                          |PE6_Pin|PE0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void st_DACDMAConfig(){
	 /* Stop DMA transfer */
	  if(HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1) != HAL_OK)
	  {
		  st_Error_Handler();
	  }

	  /* Deinitialize the DAC peripheral */
	  if(HAL_DAC_DeInit(&hdac) != HAL_OK)
	  {
		  st_Error_Handler();
	  }
	DAC_ChannelConfTypeDef sConfig = {0};

	/* Set DAC instance */
	hdac.Instance = DAC1;

	/* Initialize DAC */
	if (HAL_DAC_Init(&hdac) != HAL_OK)
	{
	/* Initialization Error */
		st_Error_Handler();
	}

	/* Config Channel */
	sConfig.DAC_Trigger                 = DAC_TRIGGER_T2_TRGO;
	sConfig.DAC_OutputBuffer            = DAC_OUTPUTBUFFER_ENABLE;
	sConfig.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
	sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;

	/* DAC Channel configuration */
	if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
	/* Channel configuration Error */
		st_Error_Handler();
	}

	/* DAC calibration */
	if (HAL_DACEx_SelfCalibrate(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		st_Error_Handler();
	}

	/* Enable DAC selected channel and associated DMA */
	if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, &DACVals[0], NVALS, DAC_ALIGN_12B_R) != HAL_OK)
	{
	/* Start DMA Error */
		st_Error_Handler();
	}
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
void st_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache (default 2-ways set associative cache)
  */
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
	  st_Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

void st_DAC1_Init()
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};
  DAC_AutonomousModeConfTypeDef sAutonomousMode = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC1;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
	  st_Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_DISABLE;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
	  st_Error_Handler();
  }

  /** Configure Autonomous Mode
  */
  sAutonomousMode.AutonomousModeState = DAC_AUTONOMOUS_MODE_DISABLE;
  if (HAL_DACEx_SetConfigAutonomousMode(&hdac, &sAutonomousMode) != HAL_OK)
  {
	  st_Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  HAL_DACEx_SelfCalibrate(&hdac, &sConfig, DAC_CHANNEL_1);
  /* USER CODE END DAC1_Init 2 */

}



void st_TIM_Start(){

	  HAL_TIM_Base_Start(&htim);
	  TIM2->DIER |= (TIM_DIER_UDE) |  TIM_DIER_CC1DE;;//(1 << 8);   // set UDE bit (update dma request enable)
	  st_DACDMAConfig();
	  HAL_TIM_Base_Start_IT(&htim);//(&htim1, TIM_CHANNEL_1);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void st_Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
