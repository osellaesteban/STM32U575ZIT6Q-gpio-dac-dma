/*
 * st_stimulator.h
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
 * Gets the global stimulator status.
 */
void stGetGlobalState(st_active_t *state);

/**
 * Enable all channels
 */

void stSetEnableAllChannels(st_active_t state);

/**
 * Sets the active state of a particular channel
 */
uint8_t stSetChannelState(uint8_t channel,st_active_t state);

/**
 * Gets the active state of a particular channel
 */
uint8_t stGetChannelState(uint8_t channel,st_active_t *state);

/**
 * Gets the label's length of a specific channel
 */
uint8_t stGetChannelLabelLength(uint8_t chann, uint16_t *length);

/**
 * Assigns a specific label to a channel
 */
uint8_t stSetChannelLabel(uint8_t chann, char *label, uint16_t length);

/**
 * Gets a specific label to a channel
 */
uint8_t stGetChannelLabel(uint8_t chann, char *label, uint16_t length);


/**
 * Determines the period, determining the ARR of the timer.
 * This has to be confirmed when the HAL is finally implemented
 */
uint8_t stSetPeriod(uint32_t period);

/**
 * Stores in period the stimulator period in counts. The stimulator
 * state is reflected in the return value.
 */
uint8_t stGetPeriod(uint32_t *period);

/**
 * Determines the channel stimulating sequence
 */
uint8_t stSetSequence(uint8_t*);

/**
 * Returns the sequence in the stimulator.
 */
uint8_t stGetSequence(uint8_t * sequence);

/**
 * Determines the GPIO port to be used by the stimulator.
 */
uint8_t stSetPort(uint32_t);

uint8_t stGetPort(uint32_t* );
/**
 * Determines the GPIO pin to be used to operate the negative port.
 */
uint8_t stSetSignPort(uint32_t);

/**
 * The sing pin is stored in ret. -1 is returned if stimulator hasn't
 * being initialized
 */
uint8_t stGetSignPin(uint32_t* ret);

/**
 * Assigns the trigger pin for the current source
 */
uint8_t stSetTriggerPin(uint32_t pin);

/**
 * Retrieves the current source trigger pin.
 */
uint8_t stGetTriggerPin(uint32_t *pin);


/**
 * Determines the Pin associated with a specific channel.
 */
uint8_t stSetChannelPin(uint8_t chan,uint32_t port);

/**
 * retrieves a channel specific activation pin
 */
uint8_t stGetChannelPin(uint8_t chan,uint32_t * pin);


uint8_t stSetChannelSignal(uint8_t ch, uint32_t sz, uint32_t *values, uint32_t *signs );

uint8_t stConfigureDefault(st_signal_type type);

void stStartStimulation();

void stStopStimulation();


#endif /* INC_ST_STIMULATOR_H_ */
