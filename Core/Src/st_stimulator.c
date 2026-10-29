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
	st_config_t config;
}st_stimulator_t;


/** Private variables */
static st_stimulator_t stimulator;


/** Functions implementations */

void stConfigureDefault(void){
	uint8_t ch_it;

	stimulator.config.stPeriod = DEFAULT_PERIOD_US;
	stimulator.config.stSettle = DEFAULT_SETTLE_US;
	stimulator.config.stTrigger = DEFAULT_TRIGGER_US;
	stimulator.config.stDs8rDelay = DEFAULT_DS8R_DELAY_US;
	stimulator.config.stPulse = DEFAULT_PULSE_US;
	stimulator.config.stTail = DEFAULT_TAIL_US;
	stimulator.config.stDacFullScale = DAC_MAX_COUNTS;
	stimulator.config.stSeqLength = CHAN_LENGTH;

	for (ch_it = 0; ch_it < CHAN_LENGTH; ch_it++){
		stimulator.config.stSequence[ch_it] = ch_it;
		stimulator.channels[ch_it].stAmplitude = 0;
		stimulator.channels[ch_it].stActiveState = st_disabled;
		stimulator.channels[ch_it].stPolarity = st_pol_positive;
		memcpy(stimulator.channels[ch_it].stLabel, "Channel 0", sizeof("Channel 0"));
		stimulator.channels[ch_it].stLabel[8] = (char)('0' + ch_it);
	}
}


st_status_t stSetChannelState(uint8_t channel, st_active_t state){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	if ((state != st_disabled) && (state != st_enabled))
		return ST_ERR_RANGE;
	stimulator.channels[channel].stActiveState = (uint8_t)state;
	return ST_OK;
}


st_status_t stGetChannelState(uint8_t channel, st_active_t *state){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	*state = (st_active_t)stimulator.channels[channel].stActiveState;
	return ST_OK;
}


st_status_t stSetChannelAmplitude(uint8_t channel, uint16_t amplitude){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	if (amplitude > AMP_MAX)
		return ST_ERR_RANGE;
	stimulator.channels[channel].stAmplitude = amplitude;
	return ST_OK;
}


st_status_t stGetChannelAmplitude(uint8_t channel, uint16_t *amplitude){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	*amplitude = stimulator.channels[channel].stAmplitude;
	return ST_OK;
}


st_status_t stSetChannelPolarity(uint8_t channel, st_polarity_t polarity){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	if (polarity > st_pol_biphasic)
		return ST_ERR_RANGE;
	stimulator.channels[channel].stPolarity = (uint8_t)polarity;
	return ST_OK;
}


st_status_t stGetChannelPolarity(uint8_t channel, st_polarity_t *polarity){
	if (channel >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	*polarity = (st_polarity_t)stimulator.channels[channel].stPolarity;
	return ST_OK;
}


st_status_t stSetChannelLabel(uint8_t chann, const char *label){
	size_t length;
	if (chann >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	length = strlen(label);
	if (length >= MAX_LABEL)
		return ST_ERR_RANGE;
	memcpy(stimulator.channels[chann].stLabel, label, length + 1);
	return ST_OK;
}


st_status_t stGetChannelLabel(uint8_t chann, char *label, uint16_t length){
	size_t needed;
	if (chann >= CHAN_LENGTH)
		return ST_ERR_CHANNEL;
	needed = strlen(stimulator.channels[chann].stLabel) + 1;
	if (length < needed)
		return ST_ERR_RANGE;
	memcpy(label, stimulator.channels[chann].stLabel, needed);
	return ST_OK;
}


st_status_t stSetPeriod(uint32_t period){
	if ((period == 0) || (period > MAX_PERIOD_US))
		return ST_ERR_RANGE;
	stimulator.config.stPeriod = period;
	return ST_OK;
}


uint32_t stGetPeriod(void){
	return stimulator.config.stPeriod;
}


st_status_t stSetSequence(const uint8_t *sequence, uint8_t length){
	uint8_t it;
	if ((length == 0) || (length > MAX_SLOTS))
		return ST_ERR_RANGE;
	for (it = 0; it < length; it++){
		if (sequence[it] >= CHAN_LENGTH)
			return ST_ERR_CHANNEL;
	}
	memcpy(stimulator.config.stSequence, sequence, length);
	stimulator.config.stSeqLength = length;
	return ST_OK;
}


uint8_t stGetSequence(uint8_t *sequence){
	memcpy(sequence, stimulator.config.stSequence, stimulator.config.stSeqLength);
	return stimulator.config.stSeqLength;
}


st_status_t stSetTiming(uint16_t settle, uint16_t trigger, uint16_t ds8rDelay, uint16_t tail){
	if ((settle < MIN_EVENT_GAP_US) || (trigger < MIN_EVENT_GAP_US) || (tail < MIN_EVENT_GAP_US))
		return ST_ERR_RANGE;
	stimulator.config.stSettle = settle;
	stimulator.config.stTrigger = trigger;
	stimulator.config.stDs8rDelay = ds8rDelay;
	stimulator.config.stTail = tail;
	return ST_OK;
}


st_status_t stSetPulseWidth(uint16_t pulse){
	if ((pulse < DS8R_PW_MIN_US) || (pulse > DS8R_PW_MAX_US))
		return ST_ERR_RANGE;
	stimulator.config.stPulse = pulse;
	return ST_OK;
}


st_status_t stSetDacFullScale(uint16_t counts){
	if ((counts == 0) || (counts > DAC_MAX_COUNTS))
		return ST_ERR_RANGE;
	stimulator.config.stDacFullScale = counts;
	return ST_OK;
}


const st_config_t *stGetConfig(void){
	return &stimulator.config;
}


const st_channel_t *stGetChannel(uint8_t channel){
	if (channel >= CHAN_LENGTH)
		return NULL;
	return &stimulator.channels[channel];
}


void stComputeSlotPlan(const st_config_t *config, st_slot_plan_t *plan){
	plan->trigOn = config->stSettle;
	plan->trigOff = plan->trigOn + config->stTrigger;
	plan->flip = plan->trigOn + config->stDs8rDelay + config->stPulse / 2;
	plan->gateOff = plan->trigOn + config->stDs8rDelay + config->stPulse + config->stTail;
	plan->spacing = (config->stSeqLength > 0) ? (config->stPeriod / config->stSeqLength) : 0;
}


st_status_t stValidate(void){
	const st_config_t *config = &stimulator.config;
	st_slot_plan_t plan;
	uint8_t it;

	if ((config->stSeqLength == 0) || (config->stSeqLength > MAX_SLOTS))
		return ST_ERR_RANGE;
	for (it = 0; it < config->stSeqLength; it++){
		if (config->stSequence[it] >= CHAN_LENGTH)
			return ST_ERR_CHANNEL;
	}
	if ((config->stPulse < DS8R_PW_MIN_US) || (config->stPulse > DS8R_PW_MAX_US))
		return ST_ERR_RANGE;
	if ((config->stDacFullScale == 0) || (config->stDacFullScale > DAC_MAX_COUNTS))
		return ST_ERR_RANGE;

	stComputeSlotPlan(config, &plan);

	// every event must be far enough from the previous one for the ISR
	if ((plan.trigOn < MIN_EVENT_GAP_US) || ((plan.trigOff - plan.trigOn) < MIN_EVENT_GAP_US))
		return ST_ERR_TIMING;
	// the biphasic flip happens after the trigger falling edge; it is checked
	// for every configuration because polarity can be changed while running
	if (plan.flip < (plan.trigOff + MIN_EVENT_GAP_US))
		return ST_ERR_TIMING;
	if ((plan.gateOff - plan.flip) < MIN_EVENT_GAP_US)
		return ST_ERR_TIMING;
	// the bridge must be open for a while before the next slot
	if (plan.spacing < (plan.gateOff + MIN_IDLE_US))
		return ST_ERR_TIMING;
	// the DS8R can't be triggered faster than its maximum rate
	if (plan.spacing < DS8R_MIN_TRIG_INTERVAL_US)
		return ST_ERR_TIMING;
	return ST_OK;
}


uint16_t stAmplitudeToDac(const st_config_t *config, uint16_t amplitude){
	if (amplitude > AMP_MAX)
		amplitude = AMP_MAX;
	return (uint16_t)(((uint32_t)amplitude * config->stDacFullScale + AMP_MAX / 2) / AMP_MAX);
}
