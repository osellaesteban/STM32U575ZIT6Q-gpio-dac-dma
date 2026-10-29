/*
 * st_channel.h
 *
 * Each stimulation channel description.
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */

#ifndef INC_ST_CHANNEL_H_
#define INC_ST_CHANNEL_H_
/** Global includes */
#include <stdint.h>

/** user includes **/
#include "st_definitions.h"


/** public structure definitions**/
/**
 * The real time fields (amplitude, state, polarity) are read by the
 * stimulation engine at the beginning of every slot, so they can be
 * changed while stimulating. Each one is written with a single store.
 */
typedef struct 	st_channel{
	volatile uint16_t stAmplitude;	// per mil of the DS8R full scale
	volatile uint8_t stActiveState;	// st_active_t
	volatile uint8_t stPolarity;	// st_polarity_t
	char stLabel[MAX_LABEL];
}st_channel_t;

#endif /* INC_ST_CHANNEL_H_ */
