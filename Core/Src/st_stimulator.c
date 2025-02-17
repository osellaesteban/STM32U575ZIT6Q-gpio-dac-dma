/*
 * stimulator.c
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */
#include <st_stimulator.h>
#include <string.h>
#include "st_DMA_HAL.h"

/** private structure definitions **/


typedef struct 	st_stimulator{
	st_channel_t channels[N_CHAN];
	uint32_t stPeriod;
	uint8_t stStimSequence[N_CHAN];
	uint32_t stPort;
	uint32_t stSignPin;
	uint32_t stTriggerPin;
	st_active_t stGlobalState;
	uint32_t stStimSize;
}st_stimulator_t;


/** Private variables */
st_stimulator_t stimulator;

uint32_t stDACVals[GPIO_DAC_SIZE];
uint32_t stGPIOVals[GPIO_DAC_SIZE];


uint32_t pins[8] = {CH0_Pin, CH1_Pin, CH2_Pin, CH3_Pin,
		CH4_Pin, CH5_Pin, CH6_Pin, CH7_Pin};


/**
 * private Function definitions
 */
/** Functions implementations */
void stUpdateVectors();
/**
 * Determines the global state.
 * ToDo: remains to implement the actions related with the
 * new state.
 */

void stConfigure(){

}

void stInitilize(){
	stConfigureDefault(st_ramp);
	stSetEnableAllChannels(st_enabled);
	stUpdateVectors();
	st_HALIinitilize();

}

void stStartStimulation(){
	stSetGlobalState(st_enabled);
	st_DMA_Start();
}

void stStopStimulation(){
	stSetGlobalState(st_disabled);
	st_DMA_Stop();
}

void stUpdateVectors(){
	uint32_t pos = 0, it2,it =0, len  = 0, actCh=0;
	uint32_t gpios_vals[MAX_SIGNAL_LENGTH];
	for(it = 0 ; it < N_CHAN ; it++){
		if(stimulator.channels[it].stActiveState == st_enabled){
			for(it2 = 0; it2< SEP_SIZE; it2++){
				stDACVals[pos + it2] = 0;
				stGPIOVals[pos + it2] = 0;
			}
			actCh = stimulator.stStimSequence[it];
			len = stimulator.channels[actCh].stimulus.lastVal;

			for(it2 = 0 ; it2 < len ; it2++)
				gpios_vals[it2] = ( (uint32_t) (stimulator.channels[actCh].stimulus.sign[it2]>0) * stimulator.stSignPin)  |  stimulator.stTriggerPin | stimulator.channels[actCh].stPin;

			memcpy(&stDACVals[pos+SEP_SIZE] , stimulator.channels[actCh].stimulus.intensity , len*sizeof(uint32_t));
			memcpy(&stGPIOVals[pos+SEP_SIZE], gpios_vals									, len*sizeof(uint32_t));
			pos += SEP_SIZE + len;
		}
	}
	stimulator.stStimSize = pos;
}

void stSetGlobalState(st_active_t state){

	stimulator.stGlobalState = state;

}

void stGetGlobalState(st_active_t *state){
	*state = stimulator.stGlobalState;
}

void stSetEnableAllChannels(st_active_t state){
	uint32_t it;
	for(it = 0; it < N_CHAN; it++)
		stimulator.channels[it].stActiveState = state;
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
		if (chann < N_CHAN){
			if (length < MAX_LABEL)// (uint8_t)*
				memcpy( label, stimulator.channels[chann].stLabel, length);
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
		if(*pin < MAX_PIN)
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
uint8_t stSetChannelSignal(uint8_t ch, uint32_t sz, uint32_t *values, uint32_t *signs ){
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
	uint8_t ch_it,res = 0;

	uint32_t signal[MAX_SIGNAL_LENGTH];
	uint32_t sign[MAX_SIGNAL_LENGTH] = {[0 ... (MAX_SIGNAL_LENGTH-1)] = 1};
	char label[MAX_LABEL];

	// Trigger and direction pin
	stimulator.stTriggerPin = TRIGGER_Pin;
	stimulator.stSignPin = DIR_Pin;

	res += stSetPort(DEFAULT_PORT);
	res += stSetPeriod(MAX_PERIOD-1);
	stSetGlobalState(st_disabled);


	memset(sign, 0, (MAX_SIGNAL_LENGTH* sizeof(uint32_t)/2));
	memset(signal, 0, (uint32_t) sizeof(signal));



	switch (type) {
	case st_square:
		memset(signal,0xFFAA,(uint32_t) sizeof(signal)*sizeof(uint32_t)/4);
		memset(&signal[(uint32_t) MAX_SIGNAL_LENGTH/2],0xFFAA,(uint32_t) (N_CHAN* sizeof(uint16_t)/4));
		break;
	case st_ramp:
		for(uint32_t it = 0; it < MAX_SIGNAL_LENGTH; it++){
			signal[it] = (2*it*4096/MAX_SIGNAL_LENGTH & 0x0FFF);
			}
		break;
	default:
		break;
	}

	strcpy(label,"Channel ");
	for (ch_it = 0; ch_it < N_CHAN; ch_it++){
		stimulator.stStimSequence[ch_it] = ch_it;

		res += stSetChannelSignal(ch_it, (MAX_SIGNAL_LENGTH), signal, sign);
		label[8] = (ch_it+48);
		strcpy(stimulator.channels[ch_it].stLabel , label); // possibly copying garbage
		res += stSetChannelPin(ch_it,pins[ch_it]);
		res += stSetChannelState(ch_it, st_disabled);
		stimulator.channels[ch_it].stimulus.lastVal = MAX_SIGNAL_LENGTH;
	}
	return res;
}
