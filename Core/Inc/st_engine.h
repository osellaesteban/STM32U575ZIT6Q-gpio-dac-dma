/*
 * st_engine.h
 *
 *  Stimulation engine: executes the frames described in st_stimulator
 *  using TIM2 (32 bits, 1 us tick) in output compare mode. Each compare
 *  interrupt writes the GPIO lines (single BSRR write) and the DAC and
 *  programs the next event, so the event timing is set by hardware and
 *  only the ISR entry latency (< 1 us at 160 MHz) adds jitter.
 *
 *  Amplitude, state and polarity of each channel are read at the start of
 *  every slot, so they can be changed while stimulating. The rest of the
 *  configuration is copied when the stimulation starts.
 */

#ifndef INC_ST_ENGINE_H_
#define INC_ST_ENGINE_H_

#include <stdint.h>
#include "st_definitions.h"

/**
 * Configures TIM2, the DAC and the multiplexer GPIO lines. Leaves the
 * outputs in the safe state (bridges disabled, trigger low, DAC = 0).
 */
void stEngineInit(void);

/**
 * Validates the configuration and starts stimulating. frames = 0 means
 * continuous stimulation, otherwise it stops after that number of frames.
 */
st_status_t stEngineStart(uint32_t frames);

/**
 * Stops immediately and returns to the safe state.
 */
void stEngineStop(void);

uint8_t stEngineIsRunning(void);

/**
 * Frames completed since the last start.
 */
uint32_t stEngineFrameCount(void);

/**
 * Returns 1 once after a finite stimulation has finished by itself.
 */
uint8_t stEngineTakeDoneFlag(void);

/**
 * Register level safe state, usable from fault handlers.
 */
void stEngineEmergencyStop(void);

#endif /* INC_ST_ENGINE_H_ */
