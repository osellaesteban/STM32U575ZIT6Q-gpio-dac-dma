/*
 * st_DMA_HAL.h
 * Defines and implements the hardware functionalities for the stimulation,
 * using the stm32u575i-q evaluation board
 *  Created on: Jan 22, 2025
 *      Author: Osella Esteban
 *
 */

#ifndef INC_ST_DMA_HAL_H_
#define INC_ST_DMA_HAL_H_


/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#include "stm32u5xx_hal_dac.h"

#include "st_definitions.h"
#include "st_DMA_HAL.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
// Channel enable GPIO ports and pins
#define DMA_GPIO_PORT	PORTE

#define CH0_Pin GPIO_PIN_0
#define CH0_Port DMA_GPIO_PORT
#define CH1_Pin GPIO_PIN_2
#define CH1_Port DMA_GPIO_PORT
#define CH2_Pin GPIO_PIN_3
#define CH2_Port DMA_GPIO_PORT
#define CH3_Pin GPIO_PIN_4
#define CH3_Port DMA_GPIO_PORT
#define CH4_Pin GPIO_PIN_5
#define CH4_Port DMA_GPIO_PORT
#define CH5_Pin GPIO_PIN_6
#define CH5_Port DMA_GPIO_PORT
#define CH6_Pin GPIO_PIN_7
#define CH6_Port DMA_GPIO_PORT
#define CH7_Pin GPIO_PIN_8
#define CH7_Port DMA_GPIO_PORT

// Direction GPIO port and pin
#define DIR_Pin GPIO_PIN_9
#define DIR_Port GPIO_PORT

// Trigger GPIO port and pin
#define TRIGGER_Pin GPIO_PIN_10
#define TRIGGER_Port DMA_GPIO_PORT

// GPDMA Channels
#define GPIO_GPDMA	st_ch11
#define DAC_GPDMA	st_ch10


/** Port E are not exposed in the board, so I should use A, B or C.
 * A port shares the dac output with GPIOA4.
 * B and C shares an LED on 7. And PC13 is used by the button. So, I should use
 * Port B, omitting the 7th.
 *
 */
//#define PE3_Pin GPIO_PIN_3
//#define PE3_GPIO_Port GPIOE
//#define PE4_Pin GPIO_PIN_4
//#define PE4_GPIO_Port GPIOE
//#define PE5_Pin GPIO_PIN_5
//#define PE5_GPIO_Port GPIOE
//#define PE6_Pin GPIO_PIN_6
//#define PE6_GPIO_Port GPIOE

#define GPIO_DAC_SIZE NV

// Stimulation timer
#define TIM ST_TIM
#define PSK ST_PSK
#define PERIOD ST_PERIOD
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */
void st_HALIinitilize();
void st_DMA_Start();
void st_DMA_Stop();
void st_GPIO_Init();
void st_GPDMA_Init();
void st_ICACHE_Init();
void st_DAC_Init();
void st_TIM_Init();
void st_DACDMAConfig();


#endif /* INC_ST_DMA_HAL_H_ */
