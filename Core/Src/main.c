/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/

#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "linked_list.h"

#include <st_stimulus.h>
#include "st_stimulator.h"
#include "st_TxRx_headers.h""
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NVALS 16
#define STX 0xAA
#define ETX 0x55
#define HEADER_SIZE 4
#define BUFF_SIZE	9710+16

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
__IO uint32_t BspButtonState = BUTTON_RELEASED;

DAC_HandleTypeDef hdac1;

DMA_HandleTypeDef handle_GPDMA1_Channel11;
DMA_HandleTypeDef handle_GPDMA1_Channel10;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;


/* USER CODE BEGIN PV */

volatile enum {
    RX_IDLE,
    RX_WAIT_HEADER,
    RX_WAIT_PAYLOAD
} rx_state = RX_IDLE;

extern uint32_t DACVals[NV];/* = {000,4000,0000,4000,
						   000,000,4000,4000,
						   000,000,00,00,
						   4000,4000,4000,4000};

*/


extern uint32_t GPIOEVals[NV];/* = {
		PE0_Pin, 	       PE6_Pin, 		  PE2_Pin, 			 PE3_Pin,
		PE4_Pin,		   PE5_Pin, 	      PE0_Pin | PE6_Pin, PE0_Pin | PE2_Pin,
		PE0_Pin | PE3_Pin, PE0_Pin | PE4_Pin, PE0_Pin | PE5_Pin, PE6_Pin | PE2_Pin,
		PE6_Pin | PE3_Pin, PE3_Pin | PE6_Pin, PE6_Pin | PE4_Pin, PE6_Pin | PE5_Pin};
*/
extern DMA_QListTypeDef GPIOQueue;
extern DMA_QListTypeDef DACQueue;

uint8_t tx_buffer[BUFF_SIZE];

uint8_t rx_header[HEADER_SIZE];
uint8_t Rxbuffer[BUFF_SIZE];
//ser_status_t serStatus;
msg_header_t header;

uint8_t END_SEQ[] = {0x0A,0X0A, ST_STOP, 0x0A,0X0A};
#define END_SEQ_LEN (sizeof(END_SEQ))



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_ICACHE_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM2_Init(void);

static void MX_USART1_UART_Init(void);
void DACDMAConfig();
/* USER CODE BEGIN PFP */
void ResetReceiver(void) ;
void UART_StartReception(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  __HAL_RCC_PWR_CLK_ENABLE();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  //MX_GPIO_Init();

  MX_USART1_UART_Init();
  //MX_ICACHE_Init();

  //stInitilizeHW();

  uint8_t msg[] = "STM32 Stimulus MUX -->>\r\n";
  HAL_UART_Transmit(&huart1, msg, sizeof(msg), 200);
  MX_GPDMA1_Init();
  MX_DAC1_Init();

  stInitilizeHW();
  //MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  //DACDMAConfig();
  // HAL_UART_Receive_IT(&huart1, Rxbuffer, 1);
  stConfigureDefault(st_square); //st_ramp
  st_active_t state = st_enabled;
  stSetGlobalState(state);
  state = st_disabled;
  stGetGlobalState(&state);

  stEnableAllChannels();
  stUpdateOutput();
  MX_DACQueue_Config();
  HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel10, &DACQueue);

  MX_GPIOQueue_Config();
  HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel11, &GPIOQueue);
  HAL_DMAEx_List_Start(&handle_GPDMA1_Channel11);

  __HAL_LINKDMA(&hdac1, DMA_Handle1, handle_GPDMA1_Channel10);


  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  /*BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }*/

  /* USER CODE BEGIN BSP */

  /* -- Sample board code to send message over COM1 port ---- */
  //printf("Welcome to STM32 world !\n\r");

  /* -- Sample board code to switch on leds ---- */
  BSP_LED_On(LED_GREEN);
  BSP_LED_On(LED_BLUE);
  BSP_LED_On(LED_RED);
  DACDMAConfig();

  // HAL_UART_Receive_IT(&huart1, Rxbuffer, sizeof(header));

  UART_StartReception();
  /*HAL_TIM_Base_Start(&htim2);
  TIM2->DIER |= (TIM_DIER_UDE) |  TIM_DIER_CC1DE;;//(1 << 8);   // set UDE bit (update dma request enable)
  HAL_TIM_Base_Start_IT(&htim2);//(&htim1, TIM_CHANNEL_1);*/
  stStartStimulation();

  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* -- Sample board code for User push-button in interrupt mode ---- */
    if (BspButtonState == BUTTON_PRESSED)
    {
      /* Update button state */
      BspButtonState = BUTTON_RELEASED;
      /* -- Sample board code to toggle leds ---- */
      BSP_LED_Toggle(LED_GREEN);
      BSP_LED_Toggle(LED_BLUE);
      BSP_LED_Toggle(LED_RED);

      /* ..... Perform your action ..... */
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};
  DAC_AutonomousModeConfTypeDef sAutonomousMode = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
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
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  // HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0x0);
  /** Configure Autonomous Mode
  */
  sAutonomousMode.AutonomousModeState = DAC_AUTONOMOUS_MODE_DISABLE;
  if (HAL_DACEx_SetConfigAutonomousMode(&hdac1, &sAutonomousMode) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  HAL_DACEx_SelfCalibrate(&hdac1, &sConfig, DAC_CHANNEL_1);
  // sets DAC out to zero at start.
  __HAL_DAC_ENABLE(&hdac1,DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac1,DAC_CHANNEL_1);
  HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,0);

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPDMA1_Init(void)
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
  handle_GPDMA1_Channel11.Instance = GPDMA1_Channel11;
  handle_GPDMA1_Channel11.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
  handle_GPDMA1_Channel11.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  handle_GPDMA1_Channel11.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  handle_GPDMA1_Channel11.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
  handle_GPDMA1_Channel11.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
  if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel11) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel11, DMA_CHANNEL_NPRIV) != HAL_OK)
  {
    Error_Handler();
  }
  handle_GPDMA1_Channel10.Instance = GPDMA1_Channel10;
  handle_GPDMA1_Channel10.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
  handle_GPDMA1_Channel10.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  handle_GPDMA1_Channel10.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  handle_GPDMA1_Channel10.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
  handle_GPDMA1_Channel10.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
  if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel10) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel10, DMA_CHANNEL_NPRIV) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN GPDMA1_Init 2 */
  HAL_DAC_MspInit(&hdac1);

  /* USER CODE END GPDMA1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */
	  /** Enable instruction cache in 1-way (direct mapped cache)
	  */
	  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
	  {
	    Error_Handler();
	  }
  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache (default 2-ways set associative cache)
  */
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  // uint32_t clkfreq1 = HAL_RCC_GetPCLK1Freq();
  // uint32_t clkfreq2 = HAL_RCC_GetPCLK2Freq();

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = ST_PERIOD;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
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
                          |PE6_Pin|PE0_Pin |CH6_Pin | CH7_Pin| ENABLE_PIN | TRIGGER_PIN;
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

/* USER CODE BEGIN 4 */


/*
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint32_t err = HAL_UART_GetError(huart);
        // Log it or toggle a pin
        BSP_LED_Toggle(LED_GREEN);
        __NOP();
    }
}
*/


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	if(huart->Instance == USART1)
	{
		/*switch (serStatus){
		case ser_read_head:
			st_Rx_DecodeHeader(Rxbuffer,&header);
			serStatus = ser_read_msg;
			HAL_UART_Receive_IT(&huart1, Rxbuffer, header.length);
			break;
		case ser_read_msg:
			st_Rx_DecodeConfig(Rxbuffer,header.length);//,&stimulator);
			//st_Rx_DecodeMsg(Rxbuffer);
			serStatus = ser_read_head;
			HAL_UART_Receive_IT(&huart1, Rxbuffer, sizeof(header));
			break;
		}*/
		switch(rx_state)
		{
		case RX_WAIT_HEADER:
			header.start_byte = rx_header[0];
			header.msg_type = rx_header[1];
			header.length = (rx_header[3] << 8) | rx_header[2];
			rx_state = RX_WAIT_PAYLOAD;
			HAL_UART_Receive_IT(&huart1, &Rxbuffer,header.length);
			break;
		case RX_WAIT_PAYLOAD:
			st_Rx_DecodeConfig(Rxbuffer,header.length);
			rx_state = RX_WAIT_HEADER;
			HAL_UART_Receive_IT(&huart1, rx_header, HEADER_SIZE);
			break;
		default:
			HAL_UART_Receive_IT(&huart1, rx_header, HEADER_SIZE);
			break;

		}
	   /*uint16_t next_head = (rx_buffer.head + 1) % BUFF_SIZE;

		if (next_head != rx_buffer.tail) {
			rx_buffer.buffer[rx_buffer.head] = rx_byte;
			rx_buffer.head = next_head;
		}
		else {
			rx_buffer.overflow_count++;
		}
		HAL_UART_Receive_IT(&huart1, &rx_byte,1);*/
	}
}

void UART_StartReception(void)
{
    rx_state = RX_WAIT_HEADER;

    HAL_UART_Receive_IT(&huart1, rx_header, HEADER_SIZE);
	//HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
void ProcessMessage(uint8_t *msg, uint16_t *len){
	BSP_LED_Toggle(LED_GREEN);
}

void DACDMAConfig(){
	 /* Stop DMA transfer */
	  if(HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1) != HAL_OK)
	  {
		Error_Handler();
	  }

	  /* Deinitialize the DAC peripheral */
	  if(HAL_DAC_DeInit(&hdac1) != HAL_OK)
	  {
		Error_Handler();
	  }
	DAC_ChannelConfTypeDef sConfig = {0};

	/* Set DAC instance */
	hdac1.Instance = DAC1;

	/* Initialize DAC */
	if (HAL_DAC_Init(&hdac1) != HAL_OK)
	{
	/* Initialization Error */
		Error_Handler();
	}

	/* Config Channel */
	sConfig.DAC_Trigger                 = DAC_TRIGGER_T2_TRGO;
	sConfig.DAC_OutputBuffer            = DAC_OUTPUTBUFFER_ENABLE;
	sConfig.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
	sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;

	/* DAC Channel configuration */
	if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
	/* Channel configuration Error */
		Error_Handler();
	}

	/* DAC calibration */
	if (HAL_DACEx_SelfCalibrate(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	/* Enable DAC selected channel and associated DMA */
	if (HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, &DACVals[0], NV, DAC_ALIGN_12B_R) != HAL_OK)
	{
		/* Start DMA Error */
		Error_Handler();
	}
}


void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* _hdac){
	st_active_t state = st_undefined;
	if(_hdac == &hdac1)
	{
		stGetGlobalState(&state);
		//check if turn off request is present
		if(state == st_disabled)
		{
			st_HAL_575_StopTimer();
		}
	}
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM17 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM17) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  if (htim->Instance == TIM2) {
	  BSP_LED_Toggle(LED_BLUE);
  }
  /* USER CODE END Callback 1 */
}

static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  //huart1.Instance->CR3 |= USART_CR3_OVRDIS;
  /* USER CODE END USART1_Init 2 */

}


/**
  * @brief BSP Push Button callback
  * @param Button Specifies the pressed button
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
	static st_active_t globState = st_undefined;
	stGetGlobalState(&globState);

	uint16_t length = 0;
	if (globState == st_enabled)
		stStopStimulation();
	else
	{

		stSerialize(&tx_buffer, &length);
		HAL_UART_Transmit(&huart1, tx_buffer, sizeof(tx_buffer), 200);

		stStartStimulation();
	}
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
