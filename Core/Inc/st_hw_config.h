/*
 * st_hw_config.h
 *
 *  Hardware mapping of the multiplexer. All the lines MUST be on the same
 *  GPIO port, the engine updates them with single BSRR writes.
 *
 *  Expected wiring of each PWD5F60 (one full bridge per channel), through
 *  digital isolators because the bridge GND is the DS8R negative output:
 *
 *      SD1 = SD2   <- ST_ARM_PIN      (common to all channels)
 *      IN1         <- ST_POL_PIN      (common to all channels)
 *      IN2         <- ST_NPOL_PIN     (common to all channels)
 *      PWM1 = PWM2 <- ST_CHx_PIN      (one per channel, "channel select")
 *      VS (bus)    <- DS8R output +   OUT1/OUT2 -> electrodes
 *      GND         <- DS8R output -
 *
 *  PWD5F60 truth table (per half bridge, SD=1): IN=1 & PWM=1 -> high side
 *  on, otherwise low side on. Therefore:
 *      CHx=0          -> both low sides on: electrodes shorted to DS8R-,
 *                        bootstrap capacitors recharge.
 *      CHx=1, POL=1   -> OUT1 = DS8R+, OUT2 = DS8R-  (positive)
 *      CHx=1, NPOL=1  -> OUT1 = DS8R-, OUT2 = DS8R+  (negative)
 *      ARM=0          -> all MOSFETs off (high impedance).
 *
 *  ST_TRIG_PIN goes to the DS8R trigger input (TTL, use a buffer for the
 *  BNC cable) and the DAC output (PA4) to the DS8R control input through
 *  an amplifier (0-3.3 V -> 0-10 V).
 */

#ifndef INC_ST_HW_CONFIG_H_
#define INC_ST_HW_CONFIG_H_

#include "main.h"

#define ST_PORT				GPIOE
#define ST_PORT_CLK_ENABLE()	__HAL_RCC_GPIOE_CLK_ENABLE()
#define ST_PORT_CLK_IS_ENABLED()	((RCC->AHB2ENR1 & RCC_AHB2ENR1_GPIOEEN) != 0U)

/** Channel select lines (PWM1 = PWM2 of each bridge) */
#define ST_CH0_PIN			GPIO_PIN_0
#define ST_CH1_PIN			GPIO_PIN_2
#define ST_CH2_PIN			GPIO_PIN_3
#define ST_CH3_PIN			GPIO_PIN_4
#define ST_CH4_PIN			GPIO_PIN_5
#define ST_CH5_PIN			GPIO_PIN_6
#define ST_CH6_PIN			GPIO_PIN_7
#define ST_CH7_PIN			GPIO_PIN_8

/** Common lines */
#define ST_POL_PIN			GPIO_PIN_9		// IN1 of every bridge
#define ST_NPOL_PIN			GPIO_PIN_10		// IN2 of every bridge
#define ST_TRIG_PIN			GPIO_PIN_11		// DS8R trigger
#define ST_ARM_PIN			GPIO_PIN_12		// SD1 = SD2 of every bridge

#define ST_CH_PINS_ALL		(ST_CH0_PIN | ST_CH1_PIN | ST_CH2_PIN | ST_CH3_PIN | \
							 ST_CH4_PIN | ST_CH5_PIN | ST_CH6_PIN | ST_CH7_PIN)
#define ST_PINS_ALL			(ST_CH_PINS_ALL | ST_POL_PIN | ST_NPOL_PIN | ST_TRIG_PIN | ST_ARM_PIN)

/** DAC channel connected to the DS8R control input (PA4) */
#define ST_DAC_CHANNEL		DAC_CHANNEL_1

/** Interrupt priorities (lower number = higher priority) */
#define ST_ENGINE_IRQ_PRIO	0
#define ST_COMM_IRQ_PRIO	5

#endif /* INC_ST_HW_CONFIG_H_ */
