/*
 * st_pulse_stats.c
 *
 *  See st_pulse_stats.h.
 */
#include <stdio.h>
#include <string.h>

#include "st_pulse_stats.h"

/** Private functions */

/**
 * Returns the k-th smallest value (k = 0 is the minimum). values is
 * reordered (Wirth's selection algorithm, average O(n)).
 */
static uint16_t selectKth(uint16_t *values, int32_t n, int32_t k){
	int32_t left = 0, right = n - 1;
	while (left < right){
		uint16_t pivot = values[k];
		int32_t i = left, j = right;
		do{
			while (values[i] < pivot)
				i++;
			while (pivot < values[j])
				j--;
			if (i <= j){
				uint16_t tmp = values[i];
				values[i] = values[j];
				values[j] = tmp;
				i++;
				j--;
			}
		}while (i <= j);
		if (j < k)
			left = i;
		if (k < i)
			right = j;
	}
	return values[k];
}


/** Public functions */

st_window_t stWindowFromTimes(uint32_t tFrom, uint32_t tTo, uint32_t trim){
	st_window_t window = {0, 0};
	uint32_t from = tFrom + trim;
	uint32_t to = tTo - trim;
	uint32_t kFrom, kTo;

	if ((int32_t)(to - from) <= 0)
		return window;
	// sample k is taken at (k + 1) * Ts: the first sample at or after "from"
	// is ceil(from / Ts) - 1. Sample indexes are modulo 2^(32 - shift), which
	// is a multiple of the buffer length, so the TIM2 wrap around is harmless.
	kFrom = ((from + MON_TS_US - 1U) >> MON_TS_SHIFT) - 1U;
	kTo = ((to + MON_TS_US - 1U) >> MON_TS_SHIFT) - 1U;
	window.first = kFrom;
	window.count = (kTo - kFrom) & (0xFFFFFFFFUL >> MON_TS_SHIFT);
	return window;
}


void stPhaseStats(const volatile uint16_t *buffer, uint32_t mask, st_window_t window,
		uint16_t *scratch, st_phase_stats_t *stats){
	uint32_t n = window.count;
	uint32_t sum = 0;
	uint16_t max = 0, min = 0xFFFF;
	uint32_t it;

	if (n > MON_SCRATCH_LENGTH)
		n = MON_SCRATCH_LENGTH;
	if (n == 0){
		memset(stats, 0, sizeof(*stats));
		return;
	}
	for (it = 0; it < n; it++){
		uint16_t value = buffer[(window.first + it) & mask];
		scratch[it] = value;
		sum += value;
		if (value > max)
			max = value;
		if (value < min)
			min = value;
	}
	stats->n = (uint16_t)n;
	stats->max = max;
	stats->min = min;
	stats->mean = (uint16_t)((sum + n / 2U) / n);
	stats->median = selectKth(scratch, (int32_t)n, (int32_t)(n - 1U) / 2);	// lower median
}


void stPulseStats(const volatile uint16_t *buffer, uint32_t mask, const st_pulse_record_t *pulse,
		uint32_t trim, uint16_t *scratch, st_pulse_stats_t *stats){
	st_phase_stats_t baseline;

	stPhaseStats(buffer, mask, stWindowFromTimes(pulse->tStart, pulse->tTrig, trim), scratch, &baseline);
	stats->baseline = baseline.median;
	if (pulse->biphasic){
		stats->phases = 2;
		stPhaseStats(buffer, mask, stWindowFromTimes(pulse->tPhase1, pulse->tFlip, trim), scratch, &stats->phase[0]);
		stPhaseStats(buffer, mask, stWindowFromTimes(pulse->tFlip, pulse->tPhaseEnd, trim), scratch, &stats->phase[1]);
	}
	else{
		stats->phases = 1;
		stPhaseStats(buffer, mask, stWindowFromTimes(pulse->tPhase1, pulse->tPhaseEnd, trim), scratch, &stats->phase[0]);
		memset(&stats->phase[1], 0, sizeof(stats->phase[1]));
	}
}


int stFormatPulse(char *line, uint16_t length, const st_pulse_record_t *pulse, const st_pulse_stats_t *stats){
	int written;
	uint8_t it;
	uint8_t positive = pulse->positive;

	written = snprintf(line, length, "P %lu %u %u %u %u", (unsigned long)pulse->frame,
			(unsigned)pulse->slot, (unsigned)pulse->ch, (unsigned)pulse->amplitude,
			(unsigned)stats->baseline);
	for (it = 0; (it < stats->phases) && (written > 0) && (written < (int)length); it++){
		const st_phase_stats_t *phase = &stats->phase[it];
		written += snprintf(&line[written], (size_t)(length - written), " %c %u %u %u %u %u",
				positive ? '+' : '-', (unsigned)phase->n, (unsigned)phase->max,
				(unsigned)phase->mean, (unsigned)phase->median, (unsigned)phase->min);
		positive = !positive;	// second phase of a biphasic pulse
	}
	return written;
}
