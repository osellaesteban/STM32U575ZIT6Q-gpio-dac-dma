/*
 * stimulator.h
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 *
 *      Models the multichannel stimulator.
 *
 *      ToDo:
 *      - Design and implement the HAL regarding the DAC
 *      	and GPIO DMA
 *      - Interface and load operative configurations using uart.
 *      - Implement getters.
 *      - Implement stimulation start and stop functions.
 *      - Test implemented functions.
 */

#ifndef INC_ST_STIMULATOR_H_
#define INC_ST_STIMULATOR_H_
/** Global includes */
#include <stdint.h>

/** own includes **/
#include "st_channel.h"
#include "st_definitions.h"


/** public variables**/

/** Public functions **/
/**
 * Determines the global stimulator status.
 */
void stSetGlobalState(st_active_t state);

/**
 * Sets the active state of a particular channel
 */
uint8_t stSetChannelState(uint8_t channel,st_active_t state);

/**
 * Assigns a specific label to a channel
 */
uint8_t stSetChannelLabel(uint8_t chann, char *label,uint16_t length);

/**
 * Determines the period, determining the ARR of the timer.
 * This has to be confirmed when the HAL is finally implemented
 */
uint8_t stSetPeriod(uint32_t);

/**
 * Determines the channel stimulating sequence
 */
uint8_t stSetSequence(uint8_t*);

/**
 * Determines the GPIO port to be used by the stimulator.
 */
uint8_t stSetPort(uint32_t);

/**
 * Determines the GPIO pin to be used to operate the negative port.
 */
uint8_t stSetSignPort(uint32_t);


/**
 * Determines the Pin associated with a specific channel.
 */
uint8_t stSetChannelPin(uint8_t chan,uint32_t port);


uint8_t stSetChannelSignal(uint8_t ch, uint8_t sz, uint16_t* values, uint16_t* signs );

uint8_t stConfigureDefault(st_signal_type type);

#endif /* INC_ST_STIMULATOR_H_ */
