/*
 * st_HAL_U575.c
 *
 *  Created on: May 19, 2025
 *      Author: osell
 */


#include "st_HAL_U575.h"

#include "stm32u5xx_hal_dac.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
#include "st_definitions.h"

DAC_HandleTypeDef st_hdac;
TIM_HandleTypeDef st_htim;



void st_HAL_DAC_Init(void);
void st_HAL_GPIO_Init(void);
void st_HAL_TIM2_Init(void);

void st_HAL_575_InitilizeHW(){
	st_HAL_GPIO_Init();
	if (HAL_ICACHE_Enable() != HAL_OK)
	  {
	    Error_Handler();
	  }
	st_HAL_TIM2_Init();

}

void st_HAL_575_ConfigurePeriodicTimer(uint32_t period){

}


void st_HAL_575_StartTimer(){
	HAL_TIM_Base_Start(&st_htim);
	/*  HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel11, &GPIOQueue);
	HAL_DMAEx_List_Start(&handle_GPDMA1_Channel11);*/
	// GPIOS and DAC output
	TIM2->DIER |= (TIM_DIER_UDE) |  TIM_DIER_CC1DE;;//(1 << 8);   // set UDE bit (update dma request enable)

	//  DACDMAConfig();
	HAL_TIM_Base_Start_IT(&st_htim);
}


void st_HAL_575_StopTimer(){
	HAL_TIM_Base_Stop(&st_htim);
	HAL_TIM_Base_Stop_IT(&st_htim);
}


void st_HAL_575_StimulateSingle(){

}

void st_HAL_GPIO_Init(void)
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
  HAL_GPIO_WritePin(GPIOE, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
                          |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|ENABLE_PIN|TRIGGER_PIN, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2_Pin PE3_Pin PE4_Pin PE5_Pin
                           PE6_Pin PE0_Pin */
  GPIO_InitStruct.Pin = CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
          |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|ENABLE_PIN|TRIGGER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
  HAL_GPIO_WritePin(GPIOE, CH0_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH5_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH6_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH7_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, ENABLE_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, TRIGGER_PIN, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(GPIOE, CH0_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH3_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH4_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH5_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH6_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, CH7_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, ENABLE_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, TRIGGER_PIN, GPIO_PIN_SET);

  HAL_GPIO_WritePin(GPIOE, CH0_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH5_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH6_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, CH7_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, ENABLE_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, TRIGGER_PIN, GPIO_PIN_RESET);


/* USER CODE END MX_GPIO_Init_2 */
}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
void st_HAL_DAC_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};
  DAC_AutonomousModeConfTypeDef sAutonomousMode = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  st_hdac.Instance = DAC1;
  if (HAL_DAC_Init(&st_hdac) != HAL_OK)
  {
	  stHAL_Error_Handler();
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
  if (HAL_DAC_ConfigChannel(&st_hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
	  stHAL_Error_Handler();
  }
  // HAL_DAC_SetValue(&st_hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0x0);
  /** Configure Autonomous Mode
  */
  sAutonomousMode.AutonomousModeState = DAC_AUTONOMOUS_MODE_DISABLE;
  if (HAL_DACEx_SetConfigAutonomousMode(&st_hdac, &sAutonomousMode) != HAL_OK)
  {
	  stHAL_Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  HAL_DACEx_SelfCalibrate(&st_hdac, &sConfig, DAC_CHANNEL_1);
  // sets DAC out to zero at start.
  __HAL_DAC_ENABLE(&st_hdac,DAC_CHANNEL_1);
  HAL_DAC_Start(&st_hdac,DAC_CHANNEL_1);
  HAL_DAC_SetValue(&st_hdac,DAC_CHANNEL_1,DAC_ALIGN_12B_R,0);

  /* USER CODE END DAC1_Init 2 */

}


void st_HAL_TIM2_Init(void)
{

/* USER CODE BEGIN TIM2_Init 0 */

/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM2_Init 1 */
	// uint32_t clkfreq1 = HAL_RCC_GetPCLK1Freq();
	// uint32_t clkfreq2 = HAL_RCC_GetPCLK2Freq();

	/* USER CODE END TIM2_Init 1 */
	st_htim.Instance = TIM2;
	st_htim.Init.Prescaler = ST_PSK;
	st_htim.Init.CounterMode = TIM_COUNTERMODE_UP;
	st_htim.Init.Period = ST_PERIOD;
	st_htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	st_htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&st_htim) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&st_htim, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&st_htim, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

}


void stHAL_Error_Handler(){

}

