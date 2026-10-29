/*
 * st_stimulator.h
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 *
 *      Models the multichannel stimulator: per channel parameters and the
 *      frame configuration (period, sequence of channels and slot timing).
 *
 *      A frame is divided in evenly spaced slots, one per entry of the
 *      sequence. In each slot the selected channel bridge is closed, the
 *      DAC is set to the channel amplitude, the DS8R is triggered and the
 *      bridge is opened after the pulse:
 *
 *      slot start   settle      trig_off      flip (biphasic)   gate_off      next slot
 *          |----------|------------|----------------|----------------|-----------|
 *        gates+DAC  TRIG high   TRIG low       bridge reversed    gates open
 *
 *      This module has no hardware dependencies.
 */

#ifndef INC_ST_STIMULATOR_H_
#define INC_ST_STIMULATOR_H_
/** Global includes */
#include <stdint.h>

/** own includes **/
#include "st_channel.h"
#include "st_definitions.h"


/** public structure definitions **/
typedef struct st_config{
	uint32_t stPeriod;			// frame period [us]
	uint16_t stSettle;			// gates and DAC set before the trigger [us]
	uint16_t stTrigger;			// trigger pulse width [us]
	uint16_t stDs8rDelay;		// trigger to DS8R output delay [us]
	uint16_t stPulse;			// pulse width configured on the DS8R [us]
	uint16_t stTail;			// margin after the pulse before opening gates [us]
	uint16_t stDacFullScale;	// DAC counts that produce the DS8R full scale (10 V)
	uint8_t stSeqLength;
	uint8_t stSequence[MAX_SLOTS];
}st_config_t;

/** Event offsets inside a slot, relative to the slot start [us] */
typedef struct st_slot_plan{
	uint32_t trigOn;
	uint32_t trigOff;
	uint32_t flip;				// only used by biphasic channels
	uint32_t gateOff;
	uint32_t spacing;			// slot start to next slot start
}st_slot_plan_t;


/** Public functions **/

/**
 * Loads the default configuration: all channels disabled with zero
 * amplitude and positive polarity, sequence 0..CHAN_LENGTH-1.
 */
void stConfigureDefault(void);

/**
 * Sets / gets the active state of a particular channel
 */
st_status_t stSetChannelState(uint8_t channel, st_active_t state);
st_status_t stGetChannelState(uint8_t channel, st_active_t *state);

/**
 * Sets / gets the amplitude of a channel, in per mil of the DS8R full scale.
 */
st_status_t stSetChannelAmplitude(uint8_t channel, uint16_t amplitude);
st_status_t stGetChannelAmplitude(uint8_t channel, uint16_t *amplitude);

/**
 * Sets / gets the polarity mode of a channel.
 */
st_status_t stSetChannelPolarity(uint8_t channel, st_polarity_t polarity);
st_status_t stGetChannelPolarity(uint8_t channel, st_polarity_t *polarity);

/**
 * Assigns / retrieves a channel label. label must be '\0' terminated.
 * length is the size of the destination buffer.
 */
st_status_t stSetChannelLabel(uint8_t chann, const char *label);
st_status_t stGetChannelLabel(uint8_t chann, char *label, uint16_t length);

/**
 * Frame period in microseconds.
 */
st_status_t stSetPeriod(uint32_t period);
uint32_t stGetPeriod(void);

/**
 * Sequence of channels stimulated in each frame. A channel can appear
 * more than once to obtain a higher rate on it.
 */
st_status_t stSetSequence(const uint8_t *sequence, uint8_t length);
uint8_t stGetSequence(uint8_t *sequence);

/**
 * Slot timing, in microseconds.
 */
st_status_t stSetTiming(uint16_t settle, uint16_t trigger, uint16_t ds8rDelay, uint16_t tail);

/**
 * Pulse width configured on the DS8R. The stimulator can't set it, it is
 * used to know how long the bridge must stay closed.
 */
st_status_t stSetPulseWidth(uint16_t pulse);

/**
 * DAC counts that produce 10 V at the DS8R control input (depends on the
 * gain of the analog stage between the DAC and the DS8R).
 */
st_status_t stSetDacFullScale(uint16_t counts);

/**
 * Read only access to the whole configuration.
 */
const st_config_t *stGetConfig(void);

/**
 * Read only access to a channel. NULL if channel is out of range.
 */
const st_channel_t *stGetChannel(uint8_t channel);

/**
 * Checks that the configuration can be executed (sequence, DS8R limits,
 * event spacing). Must be ST_OK before starting the stimulation.
 */
st_status_t stValidate(void);

/**
 * Computes the event offsets of a slot from a configuration.
 */
void stComputeSlotPlan(const st_config_t *config, st_slot_plan_t *plan);

/**
 * Converts an amplitude in per mil into DAC counts.
 */
uint16_t stAmplitudeToDac(const st_config_t *config, uint16_t amplitude);

#endif /* INC_ST_STIMULATOR_H_ */
