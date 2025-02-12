/*
 * st_DMA_HAL.c
 *
 *  Created on: Jan 22, 2025
 *      Author: Osella Esteban
 */

//  System includes
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
#include "stm32u5xx_hal_def.h"

#include "stm32u5xx_hal_dac.h"
#include "stm32u5xx_hal_tim.h"


//  Own includes
#include "st_DMA_HAL.h"
#include "linked_list.h"

DAC_HandleTypeDef hdac;

DMA_HandleTypeDef hGPDMA1_GPIO;
DMA_HandleTypeDef hGPDMA1_DAC;

TIM_HandleTypeDef htim;


//
extern uint32_t stDACVals[];
extern uint32_t stGPIOVals[];

extern DMA_QListTypeDef stGPIOQueue;
extern DMA_QListTypeDef stDACQueue;


// Private function declaration
void st_DACDMAConfig();

// Function implementations

void st_Iinitilize(){

	st_GPIO_Init();
	st_GPDMA_Init();
	st_ICACHE_Init();
	st_DAC_Init();
	st_TIM_Init();
	// a partir de aca se caga.
	MX_GPIOQueue_Config();
	HAL_DMAEx_List_LinkQ(&hGPDMA1_GPIO, &stGPIOQueue);
	HAL_DMAEx_List_Start(&hGPDMA1_GPIO);

	MX_DACQueue_Config();
	HAL_DMAEx_List_LinkQ(&hGPDMA1_DAC, &stDACQueue);
	__HAL_LINKDMA(&hdac, DMA_Handle1, hGPDMA1_DAC);
}

void st_DMA_Start(){

	//HAL_TIM_Base_Start(&htim);
	/*  HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel11, &GPIOQueue);
	HAL_DMAEx_List_Start(&handle_GPDMA1_Channel11);*/
	// GPIOS and DAC output
	switch (TIM) {
		case T2:
			TIM2->DIER |= (TIM_DIER_UDE) |  TIM_DIER_CC1DE;
			break;
		default:
			break;
	}
	//(1 << 8);   // set UDE bit (update dma request enable)
	//HAL_DMA_Start(&handle_GPDMA1_Channel10,  (uint32_t)GPIOEVals, (uint32_t)&(GPIOC->ODR), 16);

	//HAL_DMA_Start(&handle_GPDMA1_Channel10,  (uint32_t *)DACVals, (uint32_t)&(DAC1->DOR1), 16);
	// HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)DACVals, NVALS, DAC_ALIGN_12B_R);
	/*  HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel10, &DACQueue);
	HAL_DMAEx_List_Start(&handle_GPDMA1_Channel10);*/
	st_DACDMAConfig();

	//Review starting from here. Also DAC output is not being shown.
	HAL_TIM_Base_Start_IT(&htim);//(&htim1, TIM_CHANNEL_1);
}

void st_DMA_Stop(){
	HAL_TIM_Base_Stop(&htim);
}

void st_GPIO_Init(void){
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
			  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	/* GPIO Ports Clock Enable */
	switch (DMA_GPIO_PORT) {
		case PORTA:
			__HAL_RCC_GPIOA_CLK_ENABLE();
			/*Configure GPIO pin Output Level */
			HAL_GPIO_WritePin(GPIOA, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
			break;
		case PORTB:
			__HAL_RCC_GPIOB_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOB, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
			break;
		case PORTC:
			__HAL_RCC_GPIOC_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOC, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
			break;
		case PORTD:
			__HAL_RCC_GPIOD_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOD, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
			break;
		case PORTE:
			__HAL_RCC_GPIOE_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOE, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
			break;
		case PORTF:
			__HAL_RCC_GPIOF_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOF, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
			break;
		default:
			__HAL_RCC_GPIOE_CLK_ENABLE();
			HAL_GPIO_WritePin(GPIOF, CH0_Pin|CH1_Pin|CH2_Pin|CH3_Pin
									  |CH4_Pin|CH5_Pin|CH6_Pin|CH7_Pin|TRIGGER_Pin|DIR_Pin, GPIO_PIN_RESET);
			HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
			break;
	}
	uint8_t iter = 0;
	uint16_t pins[] = {CH0_Pin, CH1_Pin, CH2_Pin, CH3_Pin, CH4_Pin,CH5_Pin, CH6_Pin,
			CH7_Pin, TRIGGER_Pin, DIR_Pin};
	for (iter = 0; iter < 10;iter++){
		HAL_GPIO_WritePin(GPIOE, pins[iter], GPIO_PIN_SET);
	}
	for (iter = 0; iter < 10;iter++){
		HAL_GPIO_WritePin(GPIOE, pins[iter], GPIO_PIN_RESET);
	}




}

void st_GPDMA_Init(void){

	/* USER CODE BEGIN GPDMA1_Init 0 */

	/* USER CODE END GPDMA1_Init 0 */

	/* Peripheral clock enable */
	__HAL_RCC_GPDMA1_CLK_ENABLE();

	/* GPDMA1 interrupt Init */
	switch (GPIO_GPDMA) {
		case st_ch11:
			HAL_NVIC_SetPriority(GPDMA1_Channel11_IRQn, 0, 0);
			HAL_NVIC_EnableIRQ(GPDMA1_Channel11_IRQn);
			hGPDMA1_GPIO.Instance = GPDMA1_Channel11;
			break;
		case st_ch10:
			HAL_NVIC_SetPriority(GPDMA1_Channel10_IRQn, 0, 0);
			HAL_NVIC_EnableIRQ(GPDMA1_Channel10_IRQn);
			hGPDMA1_GPIO.Instance = GPDMA1_Channel10;
		default:
			break;
	}

	switch (DAC_GPDMA) {
		case st_ch11:
			HAL_NVIC_SetPriority(GPDMA1_Channel11_IRQn, 0, 0);
			HAL_NVIC_EnableIRQ(GPDMA1_Channel11_IRQn);
			hGPDMA1_DAC.Instance = GPDMA1_Channel11;
			break;
		case st_ch10:
			HAL_NVIC_SetPriority(GPDMA1_Channel10_IRQn, 0, 0);
			HAL_NVIC_EnableIRQ(GPDMA1_Channel10_IRQn);
			hGPDMA1_DAC.Instance = GPDMA1_Channel10;
		default:
			break;
	}


	/* USER CODE BEGIN GPDMA1_Init 1 */

	/* USER CODE END GPDMA1_Init 1 */

	hGPDMA1_GPIO.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
	hGPDMA1_GPIO.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	hGPDMA1_GPIO.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	hGPDMA1_GPIO.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	hGPDMA1_GPIO.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
	if (HAL_DMAEx_List_Init(&hGPDMA1_GPIO) != HAL_OK)
	{
		stError_Handler();
	}
	if (HAL_DMA_ConfigChannelAttributes(&hGPDMA1_GPIO, DMA_CHANNEL_NPRIV) != HAL_OK)
	{
		stError_Handler();
	}
	// hGPDMA1_DAC.Instance = DAC_GPDMA;
	hGPDMA1_DAC.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
	hGPDMA1_DAC.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	hGPDMA1_DAC.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	hGPDMA1_DAC.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	hGPDMA1_DAC.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
	if (HAL_DMAEx_List_Init(&hGPDMA1_DAC) != HAL_OK)
	{
		stError_Handler();
	}
	if (HAL_DMA_ConfigChannelAttributes(&hGPDMA1_DAC, DMA_CHANNEL_NPRIV) != HAL_OK)
	{
		stError_Handler();
	}
}

void st_ICACHE_Init(void){
	if (HAL_ICACHE_Enable() != HAL_OK)
	{
		stError_Handler();
	}
}

void st_DAC_Init(void){
	DAC_ChannelConfTypeDef sConfig = {0};
	DAC_AutonomousModeConfTypeDef sAutonomousMode = {0};

	/** DAC Initialization
	*/
	hdac.Instance = DAC1;
	if (HAL_DAC_Init(&hdac) != HAL_OK)
	{
		stError_Handler();
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
		stError_Handler();
	}

	/** Configure Autonomous Mode
	*/
	sAutonomousMode.AutonomousModeState = DAC_AUTONOMOUS_MODE_DISABLE;
	if (HAL_DACEx_SetConfigAutonomousMode(&hdac, &sAutonomousMode) != HAL_OK)
	{
		stError_Handler();
	}
	/* USER CODE BEGIN DAC1_Init 2 */

	HAL_DACEx_SelfCalibrate(&hdac, &sConfig, DAC_CHANNEL_1);
}

void st_TIM_Init(void){
	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM2_Init 1 */
	uint32_t clkfreq1 = HAL_RCC_GetPCLK1Freq();
	uint32_t clkfreq2 = HAL_RCC_GetPCLK2Freq();

	/* USER CODE END TIM_Init 1 */
	switch (TIM) {
		case T2:
			htim.Instance = TIM2;
			break;
		default:
			break;
	}

	htim.Init.Prescaler = PSK;
	htim.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim.Init.Period = PERIOD;
	htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim) != HAL_OK)
	{
		stError_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim, &sClockSourceConfig) != HAL_OK)
	{
		stError_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim, &sMasterConfig) != HAL_OK)
	{
		stError_Handler();
	}
}


void st_DACDMAConfig(){
	/* Stop DMA transfer */
	if(HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	/* Deinitialize the DAC peripheral */
	if(HAL_DAC_DeInit(&hdac) != HAL_OK)
	{
		stError_Handler();
	}
	DAC_ChannelConfTypeDef sConfig = {0};

	/* Set DAC instance */
	hdac.Instance = DAC1;

	/* Initialize DAC */
	if (HAL_DAC_Init(&hdac) != HAL_OK)
	{
	/* Initialization Error */
		stError_Handler();
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
		stError_Handler();
	}

	/* DAC calibration */
	if (HAL_DACEx_SelfCalibrate(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		stError_Handler();
	}
	/**
	 * ToDo: align frequency with the GPIO_DAC_SIZE values here.
	 *
	 */
	/* Enable DAC selected channel and associated DMA */
	if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, &stDACVals[0], GPIO_DAC_SIZE, DAC_ALIGN_12B_R) != HAL_OK)
	{
		/* Start DMA Error */
		stError_Handler();
	}
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void stError_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

