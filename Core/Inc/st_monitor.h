/*
 * st_monitor.h
 *
 *  Stimulation current monitor. ADC1 samples the current continuously
 *  (MON_TS_US period) into a circular buffer. The engine reports every
 *  delivered pulse and the main loop computes per phase statistics
 *  (max, mean, median, min) and, on request, sends the whole waveform.
 *
 *  Report formats are documented in README.md.
 */

#ifndef INC_ST_MONITOR_H_
#define INC_ST_MONITOR_H_

#include <stdint.h>
#include "st_definitions.h"
#include "st_pulse_stats.h"

/**
 * Configures ADC1, its DMA and the sampling timer (TIM6). prescaler is the
 * one of TIM2, so both timers count at the same rate. The sampling timer is
 * not started.
 */
void stMonitorInit(uint32_t prescaler);

/**
 * Starts the sampling timer. Must be called right after starting TIM2 with
 * both counters at 0.
 */
void stMonitorStartSampling(void);

/**
 * Called by the engine (TIM2 interrupt) when a pulse has been delivered.
 */
void stMonitorPushPulse(const st_pulse_record_t *pulse);

/**
 * Main loop processing: statistics, reports and waveform streaming.
 */
void stMonitorProcess(void);

/**
 * Per pulse statistics reports on/off.
 */
void stMonitorSetReports(uint8_t enabled);
uint8_t stMonitorGetReports(void);

/**
 * Time removed at both ends of each phase before computing statistics.
 */
st_status_t stMonitorSetTrim(uint32_t trim);
uint32_t stMonitorGetTrim(void);

/**
 * Sends the waveform of the next delivered pulse of channel ch (-1 = any
 * channel). ST_ERR_BUSY if a waveform is already pending.
 */
st_status_t stMonitorRequestWave(int16_t ch);

/**
 * Pulses that couldn't be analyzed (queue full or samples overwritten).
 */
uint32_t stMonitorLost(void);

/**
 * 1 if the DMA position doesn't match the expected sample (samples lost).
 */
uint8_t stMonitorSyncError(void);

#endif /* INC_ST_MONITOR_H_ */
