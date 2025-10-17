/*
 * stimulator.c
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 */
#include <st_stimulator.h>
#include <string.h>

#include "st_HAL_U575.h"




/** private structure definitions **/



/** Private variables */
st_stimulator_t stimulator; //8828 bytes

uint32_t pins[N_CHAN+3] = {CH0_Pin, CH1_Pin, CH2_Pin, CH3_Pin,
		CH4_Pin, CH5_Pin, CH6_Pin, CH7_Pin, ENABLE_PIN, TRIGGER_PIN, SIGN_PIN};

uint32_t GPIOEVals[NV];
uint32_t DACVals[NV];


extern DMA_HandleTypeDef handle_GPDMA1_Channel11;
extern DMA_HandleTypeDef handle_GPDMA1_Channel10;
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
	char label[MAX_LABEL] = "                                                                ";

	res += stSetPort(DEFAULT_PORT);
	res += stSetPeriod(MAX_PERIOD-1);
	stSetGlobalState(st_disabled);


	memset(sign, 0, sizeof(sign));
	memset(signal, 0, sizeof(signal));

	for (sig_it = 0; sig_it < MAX_SIGNAL_LENGTH/2; sig_it++)
		sign[sig_it] = 1;

	switch (type) {
	case st_square:
		for (sig_it = 0; sig_it < MAX_SIGNAL_LENGTH/4; sig_it++)
		{
			signal[sig_it] = 0xFFAA;
		}
		for (sig_it = MAX_SIGNAL_LENGTH/2; sig_it < 3*MAX_SIGNAL_LENGTH/4; sig_it++)
			signal[sig_it] = 0xFFAA;

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
				GPIOEVals[(uint32_t) pos + sig_it+ SAMPLES_OFFSET] = stimulator.channels[ch_it].stPin |
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

void stStartStimulation(){
	stimulator.stGlobalState = st_enabled;
	st_HAL_575_StartTimer();

}


void stStopStimulation(){
	stimulator.stGlobalState = st_disabled;

}

void stInitilizeHW(){
	st_HAL_575_InitilizeHW();
}
/*
void stSerialize(const st_stimulator_t* stim, uint8_t* buffer, uint16_t* length)
{
    uint16_t offset = sizeof(msg_header_t);
    uint16_t act_chann= 0;
    for (uint8_t k = 0; k < N_CHAN; k++)
    	if(stim->channels[k].stActiveState == st_enabled)
    		act_chann++;

    // Serialize global header
    global_header_t gheader = {
        .active_channels = act_chann,
        .global_state = stim->stGlobalState,
        .reserved = 0
    };
    memcpy(buffer + offset, &gheader, sizeof(gheader));
    offset += sizeof(gheader);

    // Serialize each channel
    for(int i = 0; i < N_CHAN; i++) {
        channel_header_t cheader = {
            .active_state = stim->channels[i].stActiveState,
            .pin = stim->channels[i].stPin,
            .stimulus_length = stim->channels[i].stimulus.lastVal // Or actual length
        };
        memcpy(buffer + offset, &cheader, sizeof(cheader));
        offset += sizeof(cheader);

        // Serialize stimulus
        memcpy(buffer + offset, stim->channels[i].stimulus.intensity,
        		cheader.stimulus_length  * sizeof(uint32_t));
        offset += cheader.stimulus_length  * sizeof(uint32_t);

        memcpy(buffer + offset, stim->channels[i].stimulus.sign,
        		cheader.stimulus_length  * sizeof(uint32_t));
        offset += cheader.stimulus_length  * sizeof(uint32_t);

        // Serialize label (null-terminated)
        strncpy((char*)(buffer + offset), stim->channels[i].stLabel, MAX_LABEL);
        offset += MAX_LABEL;
    }
    // Serialize global parameters
    memcpy(buffer + offset, &stim->stPeriod, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Update message header
    msg_header_t* header = (msg_header_t*)buffer;
    header->start_byte = 0xAA;
    header->msg_type = CMD_SET_CONFIG;
    header->length = offset - sizeof(msg_header_t);
    *length = offset;
}*/


void stSerialize(uint8_t* buffer, uint16_t* length)
{
    uint16_t offset = sizeof(msg_header_t);
    uint16_t act_chann= 0;
    for (uint8_t k = 0; k < N_CHAN; k++)
    	if(stimulator.channels[k].stActiveState == st_enabled)
    		act_chann++;

    // Serialize global header
    global_header_t gheader = {
        .active_channels = act_chann,
        .global_state = stimulator.stGlobalState,
		.period = stimulator.stPeriod,
		.stim_port = stimulator.stPort,
		.stim_Trigger_pin = stimulator.stTriggerPin,
		.sign_pin = stimulator.stSignPin,
		.reserved = 0xCFCF
    };
    memcpy(buffer + offset, &gheader, sizeof(gheader));
    offset += sizeof(gheader);

    // stimulation sequence
    memcpy(buffer + offset, stimulator.stStimSequence,
            		act_chann  * sizeof(uint8_t));
    offset += act_chann  * sizeof(uint8_t);

    // Serialize each channel
    for(int i = 0; i < N_CHAN; i++) {
        channel_header_t cheader = { // possibly should include the nr of the channel
            .active_state = stimulator.channels[i].stActiveState,
            .pin = stimulator.channels[i].stPin,
            .stimulus_length = stimulator.channels[i].stimulus.lastVal // Or actual length
        };
        memcpy(buffer + offset, &cheader, sizeof(cheader));
        offset += sizeof(cheader);

        // Serialize stimulus
        memcpy(buffer + offset, stimulator.channels[i].stimulus.intensity,
        		cheader.stimulus_length  * sizeof(uint32_t));
        offset += cheader.stimulus_length  * sizeof(uint32_t);

        memcpy(buffer + offset, stimulator.channels[i].stimulus.sign,
        		cheader.stimulus_length  * sizeof(uint32_t));
        offset += cheader.stimulus_length  * sizeof(uint32_t);

        // Serialize label (null-terminated)
        strncpy((char*)(buffer + offset), stimulator.channels[i].stLabel, MAX_LABEL);
        offset += MAX_LABEL;
    }
    // Serialize global parameters
    memcpy(buffer + offset, &stimulator.stPeriod, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Update message header
    msg_header_t* header = (msg_header_t*)buffer;
    header->start_byte = 0xAA;
    header->msg_type = CMD_SET_CONFIG;
    header->length = offset - sizeof(msg_header_t);
    *length = offset;
}


uint8_t stDeserialize(const uint8_t* buffer, st_stimulator_t* stim)
{
    // Verify CRC and frame bytes first

	uint8_t res = 0;
    const global_header_t* gheader = (global_header_t*)(buffer + sizeof(msg_header_t));
    uint16_t offset = sizeof(msg_header_t) + sizeof(global_header_t);

    stim->stGlobalState = gheader->global_state;

    for(int i = 0; i < gheader->active_channels; i++) {
        const channel_header_t* cheader = (channel_header_t*)(buffer + offset);
        offset += sizeof(channel_header_t);

        stim->channels[i].stActiveState = cheader->active_state;
        stim->channels[i].stPin = cheader->pin;

        // Deserialize stimulus
        memcpy(stim->channels[i].stimulus.intensity, buffer + offset,
              MAX_SIGNAL_LENGTH * sizeof(uint32_t));
        offset += MAX_SIGNAL_LENGTH * sizeof(uint32_t);

        memcpy(stim->channels[i].stimulus.sign, buffer + offset,
              MAX_SIGNAL_LENGTH * sizeof(uint32_t));
        offset += MAX_SIGNAL_LENGTH * sizeof(uint32_t);

        // Deserialize label
        strncpy(stim->channels[i].stLabel, (const char*)(buffer + offset), MAX_LABEL);
        offset += MAX_LABEL;
    }

    // Deserialize global parameters
    stim->stPeriod = *(uint32_t*)(buffer + offset);
    offset += sizeof(uint32_t);

    return res;
}

uint8_t st_Rx_DecodeConfig(uint8_t * buff,uint16_t len){
	uint8_t res = 0;
	uint8_t active_ch = 0;
	uint16_t reserved =0;
	if (buff[0] > N_CHAN)
		res += 1<<0;

	if(res == 0)
	{
		uint8_t ch_it = 0;
		uint16_t b_it = 0;
		uint16_t signal_it = 0;
		active_ch = buff[0]; // terminar de corregir como leer esto.
		stimulator.stGlobalState = buff[1];
		stimulator.stPeriod = (buff[5]<<8*3)+(buff[4]<<8*2)+(buff[3]<<8*1)+buff[2]; // ojo que es uint32 y estamos metiendole uint8_t
		stimulator.stPort = (buff[9]<<8*3)+(buff[8]<<8*2)+(buff[7]<<8*1)+buff[6];
		stimulator.stTriggerPin = (buff[13]<<8*3)+(buff[12]<<8*2)+(buff[11]<<8*1)+buff[10];
		stimulator.stSignPin = (buff[17]<<8*3)+(buff[16]<<8*2)+(buff[15]<<8*1)+buff[14];
		reserved = (buff[19]<<8) | buff[18];

		b_it = 20;
		if (b_it + active_ch <= len)
		{
			for(ch_it = 0; ch_it < active_ch;ch_it++ )
				stimulator.stStimSequence[ch_it] = buff[ch_it + b_it];
		}
		else
			res +=(1<<6);
		b_it +=active_ch;

		for(ch_it = 0; ch_it < active_ch;ch_it++ )
		{
			stimulator.channels[ch_it].stActiveState = buff[b_it];
			b_it++;
			stimulator.channels[ch_it].stPin= buff[b_it] | (buff[b_it+1]<<8) | (buff[b_it+2]<<16) | (buff[b_it+3]<<24);
			b_it+=4;
			stimulator.channels[ch_it].stimulus.lastVal = buff[b_it] | (buff[b_it+1]<<8) ;
			b_it+=2;
			if(b_it + 2*stimulator.channels[ch_it].stimulus.lastVal < len){
				for (signal_it = 0; signal_it < stimulator.channels[ch_it].stimulus.lastVal ; signal_it++)
				{
					stimulator.channels[ch_it].stimulus.intensity[signal_it] = buff[b_it] | (buff[b_it+1]<<8) | (buff[b_it+2]<<16) | (buff[b_it+3]<<24);
					b_it +=4;
				}
				for (signal_it = 0; signal_it < stimulator.channels[ch_it].stimulus.lastVal ; signal_it++)
				{
					stimulator.channels[ch_it].stimulus.sign[signal_it] = buff[b_it] | (buff[b_it+1]<<8) | (buff[b_it+2]<<16) | (buff[b_it+3]<<24);
					b_it +=4;
				}

			}
			else
				res+= ch_it*(1<<3);
			//b_it += 2*stimulator.channels[ch_it].stimulus.lastVal;
			if (b_it+MAX_LABEL<len){
				for (signal_it = 0; signal_it < MAX_LABEL ; signal_it++){
					stimulator.channels[ch_it].stLabel[signal_it] = buff[b_it+signal_it];
				}
			}
			else
				res += ch_it * (1<<5);
			b_it += MAX_LABEL;
		}
	}
	return res;


}



/**
 * To compute a CRC of the supported data, go through the following steps:
1. Enable the CRC peripheral clock via the RCC peripheral.
2. Set the CRC data register to the initial CRC value by configuring the initial CRC value
register (CRC_INIT). In the more recent STM32 Series, it is possible to chain a CRC
calculation based on the previous CRC calculation as initial value. In this case, the
CRC_IDR register (not affected by the reset bit in CRC_CR) can be used. In HAL, this
is implemented by HAL_CRC_Calculate.
3. Set the I/O reverse bit order through the REV_IN[1:0] and REV_OUT bits, respectively,
in the CRC control register (CRC_CR).
4. Set the polynomial size and coefficients through the POLYSIZE[1:0] bits in CRC control
register (CRC_CR) and CRC polynomial register (CRC_POL), respectively.
5. Reset the CRC peripheral through the Reset bit in the CRC control register (CRC_CR).
6. Set the data to the CRC data register.
7. Read the content of the CRC data register.
8. Disable the CRC peripheral clock.
In the firmware package, the CRC_usage example runs the CRC checksum code
computing an array data (DataBuffer) of 256 supported data type. For a full description,
refer to the file Readme.txt in the CRC_usage folder.
 */
