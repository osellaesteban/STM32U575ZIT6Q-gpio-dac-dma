/*
 * st_stimulus.h
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 *      Resumes the most atomic stimulus model
 */

#ifndef INC_ST_STIMULUS_H_
#define INC_ST_STIMULUS_H_

#include <st_definitions.h>
#include <stdint.h>
typedef struct st_stimulus {
	uint16_t intensity[MAX_LENGTH];
	uint16_t sign[MAX_LENGTH];
	uint16_t lastVal;
}st_stimulus_t;

#endif /* INC_ST_STIMULUS_H_ */
