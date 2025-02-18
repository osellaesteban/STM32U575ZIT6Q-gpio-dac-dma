/*
 * st_definitions.h
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
typedef enum {
	st_square,st_ramp
}st_signal_type;


typedef enum {
	T1,T2,T3,T4,T5,T6,T7
}st_timers;

typedef enum {
	PORTA, PORTB, PORTC, PORTD, PORTE, PORTF
}st_gpio_port;

typedef enum {
	st_ch1, st_ch2, st_ch3, st_ch4, st_ch5, st_ch6, st_ch7, st_ch8, st_ch9, st_ch10, st_ch11, st_ch12
}st_gpdma_ch;

// SEPARATOR BETWEEN CHANNELS
#define SEP_SIZE	10


#define MAX_SIGNAL_LENGTH 	256
#define MAX_LABEL 			64
#define N_CHAN 				8
#define MAX_PERIOD 			300
#define MIN_PERIOD 			1

#define NV					N_CHAN * (MAX_SIGNAL_LENGTH + SEP_SIZE)
#define NVALS 16

#define MAX_PORT 			10
#define MAX_PIN 			GPIO_PIN_10
#define MAX_PIN_CNT			10



#define CH0_PIN GPIO_PIN_0
#define CH1_PIN GPIO_PIN_2
#define CH2_PIN GPIO_PIN_3
#define CH3_PIN GPIO_PIN_4
#define CH4_PIN GPIO_PIN_5
#define CH5_PIN GPIO_PIN_6
#define CH6_PIN GPIO_PIN_7
#define CH7_PIN GPIO_PIN_8


#define ST_TIM	T2
#define ST_PSK	160-1
#define ST_PERIOD 	10



#define DEFAULT_PORT	PORTE

#endif /* INC_ST_DEFINITIONS_H_ */
