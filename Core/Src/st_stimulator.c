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
	uint32_t stTriggerPin;
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

void stGetGlobalState(st_active_t *state){
	*state = stimulator.stGlobalState;
}


uint8_t stSetChannelState(uint8_t channel, st_active_t state){
	uint8_t res = 0;
	if(channel < CHAN_LENGTH){
		stimulator.channels[channel].stActiveState = state;
	}
	else
		res--;
	return res;
}


uint8_t stGetChannelState(uint8_t channel, st_active_t *state){
	uint8_t res = 0;
	if(channel < CHAN_LENGTH){
		*state = stimulator.channels[channel].stActiveState;
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

uint8_t stGetChannelLabelLength(uint8_t chann, uint16_t *length){
	uint8_t res = 0;
	if (chann < CHAN_LENGTH){
		*length = sizeof(stimulator.channels[chann].stLabel) / sizeof(stimulator.channels[chann].stLabel[0]);
	}
	else
		res--;
	return res;
}

uint8_t stGetChannelLabel(uint8_t chann, char *label, uint16_t length){
	uint8_t res = 0;
		if (chann < CHAN_LENGTH){
			if (length < MAX_LABEL)
				memcpy(*label, stimulator.channels[chann].stLabel, length);
			else{
				res -=2;
			}
		}
		else{
			res--;
		}
	return res;
}


uint8_t stSetPeriod(uint32_t period){
	uint8_t res = 0;
	if ((period < MAX_PERIOD) && (period >= MIN_PERIOD) )
		stimulator.stPeriod = period;
	else
	{
		res--;
		stimulator.stGlobalState = st_undefined;
	}
	return res;
}


uint8_t stGetPeriod(uint32_t *period ){
	uint8_t res = 0;
	if (stimulator.stGlobalState != st_undefined)
		*period = stimulator.stPeriod;
	else
	{
		res--;
	}
	return res;
}


uint8_t stSetSequence(uint8_t* sequence)
{
	uint8_t res = 0;
	memcpy(stimulator.stStimSequence,sequence,CHAN_LENGTH);
	if (stimulator.stGlobalState == st_undefined)
		res--;
	return 0;
}

uint8_t stGetSequence(uint8_t * sequence){
	uint8_t res = 0;
	memcpy(sequence,stimulator.stStimSequence,CHAN_LENGTH);
	if (stimulator.stGlobalState == st_undefined)
		res--;
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

uint8_t stGetPort(uint32_t* ret){
	uint8_t res = 0;
	*ret = stimulator.stPort;
	if (stimulator.stGlobalState == st_undefined)
		res--;
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

uint8_t stGetSignPin(uint32_t* ret){
	uint8_t res = 0;
	*ret =stimulator.stSignPin;
	if (stimulator.stGlobalState == st_undefined)
		res--;
	return res;
}
uint8_t stSetTriggerPin(uint32_t pin)
{
	uint8_t res = 0;
	if (pin < MAX_PIN)
	{
		stimulator.stTriggerPin = pin;
	}
	else
		res--;
	return res;
}
uint8_t stGetTriggerPin(uint32_t* ret){
	uint8_t res = 0;
	*ret =stimulator.stTriggerPin;
	if (stimulator.stGlobalState == st_undefined)
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


uint8_t stGetChannelPin(uint8_t chan,uint32_t * pin){
	uint8_t res = 0;
	if (chan < CHAN_LENGTH){
		if(pin < MAX_PIN)
			*pin = stimulator.channels[chan].stPin ;
		else
		{
			res-=2;
			//stimulator.channels[chan].stActiveState = st_undefined;
		}
	}
	else{
		//stimulator.channels[chan].stActiveState = st_undefined;
		res--;
	}

	return res;
}
uint8_t stSetChannelSignal(uint8_t ch, uint32_t sz, uint16_t* values, uint16_t* signs ){
	uint8_t res = 0;
	if (ch < CHAN_LENGTH){
		if(sz <= MAX_SIGNAL_LENGTH){
			memcpy(stimulator.channels[ch].stimulus.intensity,values,sz* sizeof(uint16_t));
			memcpy(stimulator.channels[ch].stimulus.sign,signs,sz* sizeof(uint16_t));
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
	uint16_t signal[MAX_SIGNAL_LENGTH];
	uint16_t sign[MAX_SIGNAL_LENGTH];
	char label[MAX_LABEL];

	res += stSetPort(DEFAULT_PORT);
	res += stSetPeriod(MAX_PERIOD-1);
	stSetGlobalState(st_disabled);


	memset(sign, 0, MAX_SIGNAL_LENGTH* sizeof(uint16_t));
	memset(signal, 0, (uint32_t) MAX_SIGNAL_LENGTH* sizeof(uint16_t));

	memset(sign, 1, (uint32_t) MAX_SIGNAL_LENGTH* sizeof(uint16_t) /2);
	switch (type) {
	case st_square:
		memset(signal,0xFFAA,(uint32_t) MAX_SIGNAL_LENGTH* sizeof(uint16_t)/4);
		memset(&signal[(uint32_t) MAX_SIGNAL_LENGTH/2],0xFFAA,(uint32_t) CHAN_LENGTH* sizeof(uint16_t)/4);
		break;
	case st_ramp:
		for(uint16_t it = 0; it < MAX_SIGNAL_LENGTH; it++){
			signal[it] = it*10;
			}
		break;
	default:
		break;
	}

	strcpy(label,"Channel ");
	for (ch_it = 0; ch_it < CHAN_LENGTH; ch_it++){
		stimulator.stStimSequence[ch_it] = ch_it;
		res += stSetChannelSignal(ch_it, (MAX_SIGNAL_LENGTH), &signal, &sign);
		label[8] = (ch_it+48);
		strcpy(stimulator.channels[ch_it].stLabel , label); // possibly copying garbage
		res += stSetChannelPin(ch_it,pins[ch_it]);
		res += stSetChannelState(ch_it, st_disabled);
	}
	return res;
}
