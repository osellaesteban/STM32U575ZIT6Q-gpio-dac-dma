/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : linked_list.c
  * Description        : This file provides code for the configuration
  *                      of the LinkedList.
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
#include "linked_list.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "st_definitions.h"
/* USER CODE END Includes */

DMA_NodeTypeDef GPIOENode;
DMA_QListTypeDef GPIOQueue;
DMA_NodeTypeDef DACNode;
DMA_QListTypeDef DACQueue;


DMA_NodeTypeDef stGPIONode;
DMA_QListTypeDef stGPIOQueue;
DMA_NodeTypeDef stDACNode;
DMA_QListTypeDef stDACQueue;
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
extern uint32_t stGPIOVals[];
extern uint32_t stDACVals[];

extern uint32_t pins[];
extern uint32_t dvals[];
/* USER CODE END PM */

/**
  * @brief  DMA Linked-list GPIOQueue configuration
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef MX_GPIOQueue_Config(void)
{
  HAL_StatusTypeDef ret = HAL_OK;
  /* DMA node configuration declaration */
  DMA_NodeConfTypeDef pNodeConfig;

  /* Set node configuration ################################################*/
  pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
  pNodeConfig.Init.Request = GPDMA1_REQUEST_TIM2_UP;
  pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  pNodeConfig.Init.Direction = DMA_MEMORY_TO_PERIPH;
  pNodeConfig.Init.SrcInc = DMA_SINC_INCREMENTED;
  pNodeConfig.Init.DestInc = DMA_DINC_FIXED;
  pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
  pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
  pNodeConfig.Init.SrcBurstLength = 1;
  pNodeConfig.Init.DestBurstLength = 1;
  pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
  pNodeConfig.Init.TransferEventMode = DMA_TCEM_EACH_LL_ITEM_TRANSFER;
  pNodeConfig.TriggerConfig.TriggerMode = DMA_TRIGM_BLOCK_TRANSFER;
  pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_RISING;
  pNodeConfig.TriggerConfig.TriggerSelection = GPDMA1_TRIGGER_TIM2_TRGO;
  pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  pNodeConfig.SrcAddress = (uint32_t) stGPIOVals;
  switch (DEFAULT_PORT) {
	case PORTA:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOA->ODR);
		break;
	case PORTB:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOB->ODR);
		break;
	case PORTC:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOC->ODR);
		break;
	case PORTD:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOD->ODR);
		break;
	case PORTE:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOE->ODR);
		break;
	case PORTF:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOF->ODR);
		break;
	default:
		pNodeConfig.DstAddress = (uint32_t)&(GPIOE->ODR);
		break;
}

  pNodeConfig.DataSize = NV * sizeof(uint32_t);

  /* Build GPIOENode Node */
  ret |= HAL_DMAEx_List_BuildNode(&pNodeConfig, &stGPIONode);

  /* Insert GPIOENode to Queue */
  ret |= HAL_DMAEx_List_InsertNode_Tail(&stGPIOQueue, &stGPIONode);

  ret |= HAL_DMAEx_List_SetCircularMode(&stGPIOQueue);

   return ret;
}

/**
  * @brief  DMA Linked-list DACQueue configuration
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef MX_DACQueue_Config(void)
{
  HAL_StatusTypeDef ret = HAL_OK;
  /* DMA node configuration declaration */
  DMA_NodeConfTypeDef pNodeConfig;

  /* Set node configuration ################################################*/
  pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
  pNodeConfig.Init.Request = GPDMA1_REQUEST_DAC1_CH1;
  pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  pNodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
  pNodeConfig.Init.SrcInc = DMA_SINC_INCREMENTED;
  pNodeConfig.Init.DestInc = DMA_DINC_FIXED;
  pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
  pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
  pNodeConfig.Init.SrcBurstLength = 1;
  pNodeConfig.Init.DestBurstLength = 1;
  pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
  pNodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  pNodeConfig.SrcAddress = (uint32_t)stDACVals;
  pNodeConfig.DstAddress = (uint32_t)&(DAC1->DOR1);
  pNodeConfig.DataSize = NV * sizeof(uint32_t);

  /* Build DACNode Node */
  ret |= HAL_DMAEx_List_BuildNode(&pNodeConfig, &stDACNode);

  /* Insert DACNode to Queue */
  ret |= HAL_DMAEx_List_InsertNode_Tail(&stDACQueue, &stDACNode);

  ret |= HAL_DMAEx_List_SetCircularMode(&stDACQueue);

   return ret;
}

