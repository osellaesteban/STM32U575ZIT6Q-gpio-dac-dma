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
	st_channel_t channels[N_CHAN];
	uint32_t stPeriod;
	uint8_t stStimSequence[N_CHAN];
	uint32_t stPort;
	uint32_t stSignPin;
	uint32_t stTriggerPin;
	st_active_t stGlobalState;
}st_stimulator_t;


/** Private variables */
st_stimulator_t stimulator;

uint32_t pins[N_CHAN+3] = {CH0_Pin, CH1_Pin, CH2_Pin, CH3_Pin,
		CH4_Pin, CH5_Pin, CH6_Pin, CH7_Pin, ENABLE_PIN, TRIGGER_PIN, SIGN_PIN};

uint32_t GPIOEVals[NV];
uint32_t DACVals[NV];

/** Functions implementations */

/**
 * Determines the global state.
 * ToDo: remains to implement the actions related with the
 * new state.
 */
void stSetGlobalState(st_active_t state){

	stimulator.stGlobalState = state;

}


void stEnableAllChannels(){
	uint16_t it;
	for (it = 0; it < N_CHAN; it++)
		stimulator.channels[it].stActiveState = st_enabled;
}

void stDisableAllChannels(){
	uint16_t it;
	for (it = 0; it < N_CHAN; it++)
		stimulator.channels[it].stActiveState = st_disabled;
}

void stGetGlobalState(st_active_t *state){
	*state = stimulator.stGlobalState;
}


uint8_t stSetChannelState(uint8_t channel, st_active_t state){
	uint8_t res = 0;
	if(channel < N_CHAN){
		stimulator.channels[channel].stActiveState = state;
	}
	else
		res--;
	return res;
}


uint8_t stGetChannelState(uint8_t channel, st_active_t *state){
	uint8_t res = 0;
	if(channel < N_CHAN){
		*state = stimulator.channels[channel].stActiveState;
	}
	else
		res--;
	return res;
}


uint8_t stSetChannelLabel(uint8_t chann, char *label,uint16_t length){
	uint8_t res = 0;
	if (chann < N_CHAN){
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
	if (chann < N_CHAN){
		*length = sizeof(stimulator.channels[chann].stLabel) / sizeof(stimulator.channels[chann].stLabel[0]);
	}
	else
		res--;
	return res;
}

uint8_t stGetChannelLabel(uint8_t chann, char *label, uint16_t length){
	uint8_t res = 0;
		if (chann < N_CHAN)
			if (length < MAX_LABEL)
				memcpy(*label, stimulator.channels[chann].stLabel, length);
			else
				res -=2;
		else
			res--;
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
	memcpy(stimulator.stStimSequence,sequence,N_CHAN);
	if (stimulator.stGlobalState == st_undefined)
		res--;
	return 0;
}

uint8_t stGetSequence(uint8_t * sequence){
	uint8_t res = 0;
	memcpy(sequence,stimulator.stStimSequence,N_CHAN);
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
	if (chan < N_CHAN){
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
	if (chan < N_CHAN){
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
uint8_t stSetChannelSignal(uint8_t ch, uint32_t sz, uint32_t* values, uint32_t* signs ){
	uint8_t res = 0;
	if (ch < N_CHAN){
		if(sz <= MAX_SIGNAL_LENGTH){
			memcpy(stimulator.channels[ch].stimulus.intensity,values,sz* sizeof(uint32_t));
			memcpy(stimulator.channels[ch].stimulus.sign,signs,sz* sizeof(uint32_t));
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
	uint32_t ch_it,sig_it,res = 0;
	uint32_t signal[MAX_SIGNAL_LENGTH];
	uint32_t sign[MAX_SIGNAL_LENGTH];
	char label[MAX_LABEL];

	res += stSetPort(DEFAULT_PORT);
	res += stSetPeriod(MAX_PERIOD-1);
	stSetGlobalState(st_disabled);


	memset(sign, 0, sizeof(sign));
	memset(signal, 0, sizeof(signal));

	for (sig_it = 0; sig_it < MAX_SIGNAL_LENGTH/2; sig_it++)
	{
		sign[sig_it] = 1;
 	}
	switch (type) {
	case st_square:
		for (sig_it = 0; sig_it < MAX_SIGNAL_LENGTH/4; sig_it++)
		{
			signal[sig_it] = 0xFFAA;
		}
		for (sig_it = MAX_SIGNAL_LENGTH/2; sig_it < 3*MAX_SIGNAL_LENGTH/4; sig_it++)
		{
			signal[sig_it] = 0xFFAA;
		}
		// memset(signal,0xFFAA,(uint32_t) MAX_SIGNAL_LENGTH* sizeof(signal[0])/4);
		memset(&signal[(uint32_t) MAX_SIGNAL_LENGTH/2],0xFFAA,(uint32_t) N_CHAN* sizeof(signal[0])/4);
		break;
	case st_ramp:
		for(uint32_t it = 0; it < MAX_SIGNAL_LENGTH; it++){
			signal[it] = it*40;
			}
		break;
	default:
		break;
	}

	strcpy(label,"Channel ");
	for (ch_it = 0; ch_it < N_CHAN; ch_it++){
		stimulator.stStimSequence[ch_it] = ch_it;
		res += stSetChannelSignal(ch_it, (MAX_SIGNAL_LENGTH), &signal, &sign);
		label[8] = (ch_it+48);
		strcpy(stimulator.channels[ch_it].stLabel , label); // possibly copying garbage
		res += stSetChannelPin(ch_it,pins[ch_it]);
		res += stSetChannelState(ch_it, st_disabled);
	}
	stimulator.stSignPin = SIGN_PIN;
	return res;
}


uint8_t stUpdateOutput(){
	uint32_t ch_it, sig_it, res = 0;
	uint32_t pos = 0;
	for (ch_it = 0; ch_it < N_CHAN; ch_it++){
		if (stimulator.channels[ch_it].stActiveState == st_enabled){
			for (sig_it = 0; sig_it < stimulator.channels[ch_it].stimulus.lastVal;  sig_it ++){
				DACVals[(uint32_t) pos + sig_it] = (uint32_t) stimulator.channels[ch_it].stimulus.intensity[sig_it];
				GPIOEVals[(uint32_t) pos + sig_it] = stimulator.channels[ch_it].stPin |
						stimulator.channels[ch_it].stimulus.sign[sig_it]*stimulator.stSignPin |
						ENABLE_PIN | TRIGGER_PIN;
			}

			pos+=stimulator.channels[ch_it].stimulus.lastVal;
			for (sig_it = 0; sig_it <SEP_SIZE ; sig_it++ ){
				DACVals[(uint32_t) pos+sig_it] = 0;
				GPIOEVals[(uint32_t) pos+sig_it] = 0;
			}

			pos += (SEP_SIZE);
		}
	}
	return res;
}
