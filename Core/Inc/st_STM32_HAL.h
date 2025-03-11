/*
 * st_STM32_HAL.h
 *
 *  Created on: Mar 11, 2025
 *      Author: osell
 */

#ifndef INC_ST_STM32_HAL_H_
#define INC_ST_STM32_HAL_H_
/* Includes ------------------------------------------------------------------*/


void st_GPDMA_ConfigLink();
void st_GPDMA_Init();
void st_TIM_Init();
void st_GPIO_Init();
void st_DACDMAConfig();
void st_ICACHE_Init();
void st_DAC1_Init();

#endif /* INC_ST_STM32_HAL_H_ */
