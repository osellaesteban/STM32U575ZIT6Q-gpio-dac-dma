/*
 * stimulator.c
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */
#include <st_stimulator.h>
#include <string.h>

/** private structure definitions **/


typedef struct 	st_stimulator{
	st_channel_t channels[CHAN_LENGTH];
	uint32_t stPeriod;
	uint8_t stStimSequence[CHAN_LENGTH];
	uint32_t stPort;
	uint32_t stSignPin;
	st_active_t stGlobalState;
}st_stimulator_t;


/** Private variables */
st_stimulator_t stimulator;

uint8_t pins[CHAN_LENGTH] = {CH0_PIN, CH1_PIN, CH2_PIN, CH3_PIN,
		CH4_PIN, CH5_PIN, CH6_PIN, CH7_PIN};


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
		else{
			res -=2;
			stimulator.channels[chann].stActiveState = st_undefined;
		}
	}
	else{
		stimulator.channels[chann].stActiveState = st_undefined;
		res--;
	}
	return res;
}

uint8_t stSetPeriod(uint32_t period){
	uint8_t res = 0;
	if (period < MAX_PERIOD && period >= MIN_PERIOD )
		stimulator.stPeriod = period;
	else
	{
		res--;
		stimulator.stGlobalState = st_undefined;
	}
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
	{
		res--;
		stimulator.stGlobalState = st_undefined;
	}
	return res;
}

uint8_t stSetSignPin(uint32_t pin)
{
	uint8_t res = 0;
	if (pin < MAX_PIN)
	{
		stimulator.stSignPin = pin;
	}
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
		{
			res-=2;
			stimulator.channels[chan].stActiveState = st_undefined;
		}
	}
	else{
		stimulator.channels[chan].stActiveState = st_undefined;
		res--;
	}

	return res;
}

uint8_t stSetChannelSignal(uint8_t ch, uint8_t sz, uint16_t* values, uint16_t* signs ){
	uint8_t res = 0;
	if (ch < CHAN_LENGTH){
		if(sz < MAX_SIGNAL_LENGTH){
			memcpy(stimulator.channels[ch].stimulus.intensity,values,sz);
			memcpy(stimulator.channels[ch].stimulus.sign,signs,sz);
			stimulator.channels[ch].stimulus.lastVal = sz;
		}
		else{
			res-=2;
			stimulator.channels[ch].stActiveState = st_undefined;
		}
	}
	else{
		res--;
		stimulator.channels[ch].stActiveState = st_undefined;
	}
	return res;
}

uint8_t stConfigureDefault(st_signal_type type){
	uint8_t ch_it,res = 0;
	uint16_t signal[CHAN_LENGTH];
	uint16_t sign[CHAN_LENGTH];

	res += stSetPort(DEFAULT_PORT);
	res += stSetPeriod(MAX_PERIOD-1);
	stSetGlobalState(st_disabled);


	memset(sign, 0, (uint16_t) CHAN_LENGTH);
	memset(signal, 0, (uint16_t) CHAN_LENGTH);

	memset(sign, 1, (uint16_t) CHAN_LENGTH/2);
	switch (type) {
	case st_square:
		memset(signal,0xFFAA,(uint16_t) CHAN_LENGTH/4);
		memset(&signal[(uint16_t) CHAN_LENGTH/2],0xFFAA,(uint16_t) CHAN_LENGTH/4);
		break;
	case st_ramp:
		for(uint16_t it = 0; it < CHAN_LENGTH; it++){
			signal[it] = it*10;
			}
		break;
	default:
		break;
	}
	for (ch_it = 0; ch_it++; ch_it < CHAN_LENGTH){
		stimulator.stStimSequence[ch_it] = ch_it;
		res += stSetChannelSignal(ch_it, MAX_SIGNAL_LENGTH, signal, sign);
		strlcpy(stimulator.channels[ch_it].stLabel , strcat('Channel ',(char) (ch_it+48)),9); // possibly copying garbage
		res += stSetChannelPin(ch_it,pins[ch_it]);
		res += stSetChannelState(ch_it, st_disabled);
	}
	return res;
}
