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
 *      - Interface and load operative configurations.
 *      - Implement getters.
 *      - Implement stimulus setters.
 *      - Implement default initialization.
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
uint8_t stSetChannelLabel(uint8_t chann, char *label,uint16_t length);
uint8_t stSetPeriod(uint32_t);
uint8_t stSetSequence(uint8_t*);
uint8_t stSetPort(uint32_t);
uint8_t stSetChannelPin(uint8_t chan,uint32_t port);




#endif /* INC_ST_STIMULATOR_H_ */
