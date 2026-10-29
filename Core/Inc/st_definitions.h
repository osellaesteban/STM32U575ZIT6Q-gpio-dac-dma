/*
 * st_definitions.h
 *
 *  Created on: Jan 13, 2025
 *      Author: Osella Esteban
 *
 *  Limits and default values of the stimulator model. This header is
 *  hardware independent (no HAL includes) so the model and the command
 *  protocol can also be compiled and tested on a PC.
 *  The GPIO mapping lives in st_engine.h.
 */

#ifndef INC_ST_DEFINITIONS_H_
#define INC_ST_DEFINITIONS_H_

#include <stdint.h>

#define ST_FW_VERSION		"0.3"

/** Model sizes */
#define CHAN_LENGTH			8		// number of multiplexed channels (one PWD5F60 each)
#define MAX_SLOTS			32		// max. stimulation slots per frame (sequence length)
#define MAX_LABEL			16		// channel label, including the terminating '\0'

/** Amplitude: per mil of the DS8R full scale (0..1000 = 0..10 V at the DS8R control input) */
#define AMP_MAX				1000
#define DAC_MAX_COUNTS		4095

/** DS8R limits (see DS8R manual) */
#define DS8R_PW_MIN_US		50		// pulse width range configurable on the DS8R
#define DS8R_PW_MAX_US		2000
#define DS8R_MIN_TRIG_INTERVAL_US 1000	// 1 kHz max. repetition rate with standard firmware

/** Timing limits, all in microseconds (timer tick = 1 us) */
#define MIN_EVENT_GAP_US	5		// min. time between two consecutive engine events
#define MIN_IDLE_US			100		// min. time with all gates open between slots (bootstrap recharge)
#define MAX_PERIOD_US		10000000UL	// 10 s
#define ARM_PRECHARGE_US	2000	// SD high and low sides on before the first slot (bootstrap charge)

/** Current monitor (ADC sampling of the stimulation current) */
#define MON_TS_SHIFT		1		// sampling period = 2^MON_TS_SHIFT us (must be a power of 2)
#define MON_TS_US			(1UL << MON_TS_SHIFT)
#define MON_BUFFER_LENGTH	16384	// circular sample buffer (power of 2), 32 ms at 500 kS/s
#define MON_WAVE_MAX		4096	// max. samples of a requested waveform
#define MON_SCRATCH_LENGTH	2048	// max. samples used to compute the statistics of a phase
#define MON_TRIM_MAX_US		500
#define DEFAULT_MON_TRIM_US	4		// samples excluded at both ends of each phase (edges)

/** Defaults */
#define DEFAULT_PERIOD_US	25000	// 40 Hz frame
#define DEFAULT_SETTLE_US	50		// gates + DAC set -> trigger rising edge
#define DEFAULT_TRIGGER_US	10		// trigger pulse width
#define DEFAULT_DS8R_DELAY_US 10	// trigger -> DS8R output start (measure it!)
#define DEFAULT_PULSE_US	200		// pulse width configured on the DS8R front panel
#define DEFAULT_TAIL_US		50		// margin after the DS8R pulse before opening the gates

typedef enum {
	st_disabled = 0, st_enabled, st_undefined
}st_active_t;

/**
 * Polarity of the pulse delivered to the electrodes. It is produced by the
 * H bridge, so the DS8R must be configured as MONOPHASIC POSITIVE.
 *  - positive / negative: fixed polarity.
 *  - alternate: polarity is inverted on every pulse of that channel.
 *  - biphasic: the bridge is reversed in the middle of the DS8R pulse, so
 *    each half of the configured pulse width is one phase (experimental,
 *    verify with an oscilloscope before using it on a subject).
 */
typedef enum {
	st_pol_positive = 0, st_pol_negative, st_pol_alternate, st_pol_biphasic
}st_polarity_t;

typedef enum {
	ST_OK = 0,
	ST_ERR_CHANNEL = -1,	// channel index out of range
	ST_ERR_RANGE = -2,		// value out of range
	ST_ERR_TIMING = -3,		// timing configuration is not feasible
	ST_ERR_ARG = -4,		// malformed argument
	ST_ERR_BUSY = -5,		// not allowed while stimulating / operation pending
	ST_ERR_UNAVAILABLE = -6	// hardware not available (current monitor init failed)
}st_status_t;

#endif /* INC_ST_DEFINITIONS_H_ */
