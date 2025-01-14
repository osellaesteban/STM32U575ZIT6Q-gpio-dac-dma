/*
 * definitions.h
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */

#ifndef INC_ST_DEFINITIONS_H_
#define INC_ST_DEFINITIONS_H_
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_gpio.h"
// using a signal length of 128 and 8 channels,
// it should take up to 4kb. The uP has up 2Mb, so, it should be
// determined by the DAC temporal resolution and the AHB usage
#define MAX_SIGNAL_LENGTH 	128
#define MAX_LABEL 	64
#define CHAN_LENGTH 8
#define MAX_PERIOD 	300
#define MIN_PERIOD 	1

#define MAX_PORT 	5
#define MAX_PIN 	16

#define DEFAULT_PORT	GPIOE


#define CH0_PIN GPIO_PIN_0
#define CH1_PIN GPIO_PIN_2
#define CH2_PIN GPIO_PIN_3
#define CH3_PIN GPIO_PIN_4
#define CH4_PIN GPIO_PIN_5
#define CH5_PIN GPIO_PIN_6
#define CH6_PIN GPIO_PIN_7
#define CH7_PIN GPIO_PIN_8

typedef enum {
	st_square,st_ramp
}st_signal_type;




#endif /* INC_ST_DEFINITIONS_H_ */
