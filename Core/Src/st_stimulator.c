/*
 * stimulator.c
 *
 *  Created on: Jan 13, 2025
 *      Author: osell
 */
#include <st_stimulator.h>
#include <string.h>

/** own definitions**/


typedef struct 	st_stimulator{
	st_channel_t channels[CHAN_LENGTH];
	uint32_t stPeriod;
	uint8_t stStimSequence[CHAN_LENGTH];
	uint32_t stPort;
	st_active_t stGlobalState;
}st_stimulator_t;


/** Private variables */
st_stimulator_t stimulator;


/** Functions implementations */

/**
 * Determines the global state.
 * ToDo: remains to implement the actions related with the
 * new state.
 */
void stSetGlobalState(st_active_t state){
	stimulator.stGlobalState = state;

}

uint8_t stSetChannelState(uint8_t channel,st_active_t state){
	uint8_t res = 0;
	if(channel < CHAN_LENGTH){
		stimulator.channels[channel].stActiveState = state;
	}
	else
		res--;
	return res;
}

uint8_t stSetChannelLabel(uint8_t chann, char *label,uint16_t length){
	uint8_t res = 0;
	if (chann < CHAN_LENGTH){
		if (length < MAX_LABEL)
			memcpy(stimulator.channels[chann].stLabel, &label, length);
		else
			res -=2;
		}
	else
		res--;

	return res;
}

uint8_t stSetPeriod(uint32_t period){
	uint8_t res = 0;
	if (period < MAX_PERIOD)
		stimulator.stPeriod = period;
	else
		res--;
	return res;
}

uint8_t stSetSequence(uint8_t* sequence)
{
	memcpy(stimulator.stStimSequence,sequence,CHAN_LENGTH);
	return 0;
}

uint8_t stSetPort(uint32_t port)
{
	uint8_t res = 0;
	if (port < MAX_PORT)
		stimulator.stPort = port;
	else
		res--;
	return res;
}

uint8_t stSetChannelPin(uint8_t chan,uint32_t pin){
	uint8_t res = 0;
	if (chan < CHAN_LENGTH){
		if(pin < MAX_PIN)
			stimulator.channels[chan].stPin = pin;
		else
			res-=2;
	}
	else
		res--;
	return res;
}
