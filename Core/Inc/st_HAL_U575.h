/*
 * st_HAL_U575.h
 *
 *  Created on: May 19, 2025
 *      Author: osell
 */

#ifndef INC_ST_HAL_U575_H_
#define INC_ST_HAL_U575_H_


#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
/**
 * Initilizes the hardware
 */
void st_HAL_575_InitilizeHW();

/**
 * Configures the timer
 */
void st_HAL_575_ConfigurePeriodicTimer(uint32_t period);

/**
 * Stops the timer that triggers periodic the stimulation
 */
void st_HAL_575_StartTimer();

/**
 * Starts the timer that triggers periodic the stimulation
 */
void st_HAL_575_StopTimer();

/**
 * Sends a single stimulation period
 */
void st_HAL_575_StimulateSingle();

#endif /* INC_ST_HAL_U575_H_ */
