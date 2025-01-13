/*
 * st_channel.h
 *
 * Each stimulation channel description and operations
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */

#ifndef INC_ST_CHANNEL_H_
#define INC_ST_CHANNEL_H_
/** Global includes */
#include <stdint.h>

/** own includes **/
#include "st_stimulus.h"


/** own definitions**/
typedef enum {
	st_disabled = 0, st_enabled, st_undefined
}st_active_t;

typedef struct 	st_channel{
	st_stimulus_t stmimulus;
	char stLabel[MAX_LABEL];
	st_active_t stActiveState;
	uint32_t stPin;
}st_channel_t;

/** public variables**/

/** Public functions **/



#endif /* INC_ST_CHANNEL_H_ */
