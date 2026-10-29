/*
 * Host tests of the stimulator model, the command protocol, the engine
 * (st_engine.c is compiled against fake registers, see fake/main.h, and
 * TIM2 is simulated with a 1 us step) and the pulse statistics.
 *
 * Build and run with run_tests.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st_stimulator.h"
#include "st_protocol.h"
#include "st_engine.h"
#include "st_monitor.h"
#include "st_comm.h"
#include "st_pulse_stats.h"
#include "st_hw_config.h"

GPIO_TypeDef fakeGpioE;
TIM_TypeDef fakeTim2;
DAC_TypeDef fakeDac1;
RCC_TypeDef fakeRcc;
DAC_HandleTypeDef hdac1;
TIM_HandleTypeDef htim2;

void TIM2_IRQHandler(void);

void Error_Handler(void){
	printf("Error_Handler called\n");
	exit(1);
}

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/** Stubs of the hardware dependent monitor and serial port */
static st_pulse_record_t pushed[64];
static int pushedCount;
static uint32_t monitorPrescaler;
static uint8_t monitorReports;
static uint32_t monitorTrim = DEFAULT_MON_TRIM_US;
static int16_t monitorWave = -2;		// -2: no request

void stMonitorInit(uint32_t prescaler){ monitorPrescaler = prescaler; }
void stMonitorStartSampling(void){ }
void stMonitorPushPulse(const st_pulse_record_t *pulse){
	if (pushedCount < (int)(sizeof(pushed) / sizeof(pushed[0])))
		pushed[pushedCount++] = *pulse;
}
void stMonitorSetReports(uint8_t enabled){ monitorReports = enabled; }
uint8_t stMonitorGetReports(void){ return monitorReports; }
st_status_t stMonitorSetTrim(uint32_t trim){
	if (trim > MON_TRIM_MAX_US)
		return ST_ERR_RANGE;
	monitorTrim = trim;
	return ST_OK;
}
uint32_t stMonitorGetTrim(void){ return monitorTrim; }
st_status_t stMonitorRequestWave(int16_t ch){
	if ((ch >= CHAN_LENGTH) || (ch < -1))
		return ST_ERR_CHANNEL;
	if (monitorWave != -2)
		return ST_ERR_BUSY;
	monitorWave = ch;
	return ST_OK;
}
uint32_t stMonitorLost(void){ return 0; }
uint8_t stMonitorSyncError(void){ return 0; }
uint32_t stCommTxDropped(void){ return 0; }

/** Protocol helper */
static char response[200];
static const char *cmd(const char *text){
	char line[200];
	strncpy(line, text, sizeof(line) - 1);
	line[sizeof(line) - 1] = '\0';
	stProtocolHandleLine(line, response, sizeof(response));
	return response;
}
#define EXPECT(command, expected) do { const char *r = cmd(command); \
	if (strncmp(r, expected, strlen(expected)) != 0) { \
		printf("FAIL %s:%d: '%s' -> '%s' (expected '%s')\n", __FILE__, __LINE__, command, r, expected); failures++; } } while (0)


/** Engine simulation */
typedef struct { long t; uint32_t odr; uint32_t dac; } sample_t;
static sample_t trace[20000];
static int traceLength;
static uint32_t odr;
static long simTime;			// elapsed simulated time, TIM2->CNT wraps around

static void applyOutputs(void){
	uint32_t b = fakeGpioE.BSRR;
	if (b != 0U){
		odr &= ~(b >> 16);
		odr |= (b & 0xFFFFU);
		fakeGpioE.BSRR = 0;
	}
	if ((traceLength == 0) || (trace[traceLength - 1].odr != odr) || (trace[traceLength - 1].dac != fakeDac1.DHR12R1)){
		trace[traceLength].t = simTime;
		trace[traceLength].odr = odr;
		trace[traceLength].dac = fakeDac1.DHR12R1;
		traceLength++;
	}
}

static void simulate(uint32_t microseconds){
	while (microseconds--){
		fakeTim2.CNT++;
		simTime++;
		if (fakeTim2.CNT == fakeTim2.CCR1)
			fakeTim2.SR |= TIM_SR_CC1IF;
		if ((fakeTim2.SR & TIM_SR_CC1IF) && (fakeTim2.DIER & TIM_DIER_CC1IE))
			TIM2_IRQHandler();
		applyOutputs();
	}
}

static void resetTrace(void){
	traceLength = 0;
	applyOutputs();
}

static int countBits(uint32_t v){
	int n = 0;
	while (v){ n += v & 1U; v >>= 1; }
	return n;
}

/** Invariants that must hold on every output state */
static void checkInvariants(void){
	int i;
	for (i = 0; i < traceLength; i++){
		uint32_t o = trace[i].odr;
		CHECK(countBits(o & ST_CH_PINS_ALL) <= 1);					// never two channels at once
		if (o & ST_TRIG_PIN){
			CHECK(countBits(o & ST_CH_PINS_ALL) == 1);				// trigger only with a closed bridge
			CHECK(o & ST_ARM_PIN);
			CHECK(countBits(o & (ST_POL_PIN | ST_NPOL_PIN)) == 1);
		}
		if (o & ST_CH_PINS_ALL)
			CHECK(o & ST_ARM_PIN);
	}
}

/** Returns the time of the n-th rising edge of pin (relative), or -1 */
static long risingEdge(uint32_t pin, int n){
	int i;
	for (i = 1; i < traceLength; i++){
		if ((trace[i].odr & pin) && !(trace[i - 1].odr & pin)){
			if (n-- == 0)
				return trace[i].t;
		}
	}
	return -1;
}

static long fallingEdge(uint32_t pin, int n){
	int i;
	for (i = 1; i < traceLength; i++){
		if (!(trace[i].odr & pin) && (trace[i - 1].odr & pin)){
			if (n-- == 0)
				return trace[i].t;
		}
	}
	return -1;
}

static uint32_t stateAt(long t){
	int i;
	uint32_t o = trace[0].odr;
	for (i = 0; i < traceLength; i++){
		if (trace[i].t > t)
			break;
		o = trace[i].odr;
	}
	return o;
}

static uint32_t dacAt(long t){
	int i;
	uint32_t d = trace[0].dac;
	for (i = 0; i < traceLength; i++){
		if (trace[i].t > t)
			break;
		d = trace[i].dac;
	}
	return d;
}


static void testModel(void){
	uint8_t seq[3] = {0, 1, 8};
	char label[MAX_LABEL];

	stConfigureDefault();
	CHECK(stValidate() == ST_OK);
	CHECK(stSetChannelAmplitude(8, 10) == ST_ERR_CHANNEL);
	CHECK(stSetChannelAmplitude(0, 1001) == ST_ERR_RANGE);
	CHECK(stSetSequence(seq, 3) == ST_ERR_CHANNEL);
	CHECK(stSetSequence(seq, 0) == ST_ERR_RANGE);
	CHECK(stSetChannelLabel(0, "0123456789abcdef") == ST_ERR_RANGE);	// 16 chars + '\0' > MAX_LABEL
	CHECK(stSetChannelLabel(0, "Biceps_L") == ST_OK);
	CHECK(stGetChannelLabel(0, label, sizeof(label)) == ST_OK);
	CHECK(strcmp(label, "Biceps_L") == 0);
	CHECK(stGetChannelLabel(7, label, sizeof(label)) == ST_OK);
	CHECK(strcmp(label, "Channel 7") == 0);
	CHECK(stAmplitudeToDac(stGetConfig(), 0) == 0);
	CHECK(stAmplitudeToDac(stGetConfig(), 1000) == 4095);
	CHECK(stAmplitudeToDac(stGetConfig(), 500) == 2048);

	// 8 slots in 6 ms -> 750 us spacing < 1 ms DS8R minimum interval
	CHECK(stSetPeriod(6000) == ST_OK);
	CHECK(stValidate() == ST_ERR_TIMING);
	// pulse too long for the slot
	stConfigureDefault();
	CHECK(stSetPeriod(8000) == ST_OK);
	CHECK(stSetPulseWidth(2000) == ST_OK);
	CHECK(stValidate() == ST_ERR_TIMING);
	CHECK(stSetPulseWidth(2001) == ST_ERR_RANGE);
	stConfigureDefault();
}


static void testProtocol(void){
	stConfigureDefault();
	EXPECT("PING", "OK PONG");
	EXPECT("ping", "OK PONG");
	EXPECT("INFO", "OK STIMMUX");
	EXPECT("", "ERR ARG");
	EXPECT("FOO", "ERR UNKNOWN");
	EXPECT("CH 0 AMP 500", "OK");
	EXPECT("CH 0 AMP 1001", "ERR RANGE");
	EXPECT("CH 8 AMP 1", "ERR CHANNEL");
	EXPECT("CH 0 AMP -1", "ERR ARG");
	EXPECT("CH 0 AMP 99999999999", "ERR ARG");
	EXPECT("CH 0 EN 2", "ERR ARG");
	EXPECT("CH 0 EN 1", "OK");
	EXPECT("ch 0 pol alt", "OK");
	EXPECT("CH 0 POL XYZ", "ERR ARG");
	EXPECT("CH 0 LABEL Quad_R", "OK");
	EXPECT("CH 0", "OK CH=0 EN=1 AMP=500 POL=ALT LABEL=Quad_R");
	EXPECT("AMPS 1 2 3", "OK");
	EXPECT("CH 2", "OK CH=2 EN=0 AMP=3 POL=POS");
	EXPECT("AMPS 1 2 3 4 5 6 7 8 9", "ERR ARG");
	EXPECT("AMPS 1 2000", "ERR RANGE");
	EXPECT("CH 1", "OK CH=1 EN=0 AMP=2");				// nothing applied on error
	EXPECT("ENS 1 0 1", "OK");
	EXPECT("CH 2", "OK CH=2 EN=1");
	EXPECT("SEQ 0 1 9", "ERR CHANNEL");
	EXPECT("SEQ 0,1,2", "OK");
	EXPECT("PERIOD 30000", "OK");
	EXPECT("TIMING 40 10 15 60", "OK");
	EXPECT("TIMING 1 10 15 60", "ERR RANGE");
	EXPECT("PW 300", "OK");
	EXPECT("DACFS 3000", "OK");
	EXPECT("GET", "OK PERIOD=30000 SETTLE=40 TRIG=10 DELAY=15 PW=300 TAIL=60 DACFS=3000 SEQ=0,1,2");
	EXPECT("WDOG 500", "OK");
	CHECK(stProtocolGetWatchdog() == 500);
	EXPECT("WDOG 0", "OK");
	EXPECT("PERIOD 2000", "OK");
	EXPECT("RUN", "ERR TIMING");						// 3 slots in 2 ms
	EXPECT("PERIOD 30000", "OK");
	EXPECT("RUN 10", "OK");
	EXPECT("STATUS", "OK RUN=1");
	EXPECT("RUN", "ERR BUSY");
	EXPECT("PERIOD 40000", "ERR BUSY");
	EXPECT("SEQ 0", "ERR BUSY");
	EXPECT("CH 1 AMP 10", "OK");						// real time changes allowed
	EXPECT("STOP", "OK");
	EXPECT("STATUS", "OK RUN=0");
	EXPECT("DEFAULT", "OK");

	/* current monitor */
	EXPECT("MON 1", "OK");
	CHECK(monitorReports == 1);
	EXPECT("MON 2", "ERR ARG");
	EXPECT("MON 0", "OK");
	CHECK(monitorReports == 0);
	EXPECT("MONTRIM 10", "OK");
	CHECK(monitorTrim == 10);
	EXPECT("MONTRIM 501", "ERR RANGE");
	EXPECT("STATUS", "OK RUN=0 FRAMES=");
	EXPECT("WAVE 8", "ERR CHANNEL");
	EXPECT("WAVE 3", "OK");
	CHECK(monitorWave == 3);
	EXPECT("WAVE", "ERR BUSY");
	monitorWave = -2;
	EXPECT("WAVE", "OK");
	CHECK(monitorWave == -1);
	monitorWave = -2;
	EXPECT("WAVE x", "ERR ARG");
}


/** Reference statistics computed by sorting */
static int compareU16(const void *a, const void *b){
	return (int)*(const uint16_t *)a - (int)*(const uint16_t *)b;
}

static void referenceStats(const uint16_t *buffer, uint32_t mask, st_window_t window, st_phase_stats_t *stats){
	static uint16_t sorted[MON_SCRATCH_LENGTH];
	uint32_t it, sum = 0;
	memset(stats, 0, sizeof(*stats));
	if (window.count == 0)
		return;
	for (it = 0; it < window.count; it++){
		sorted[it] = buffer[(window.first + it) & mask];
		sum += sorted[it];
	}
	qsort(sorted, window.count, sizeof(sorted[0]), compareU16);
	stats->n = (uint16_t)window.count;
	stats->min = sorted[0];
	stats->max = sorted[window.count - 1];
	stats->median = sorted[(window.count - 1) / 2];
	stats->mean = (uint16_t)((sum + window.count / 2) / window.count);
}

static int sameStats(const st_phase_stats_t *a, const st_phase_stats_t *b){
	return (a->n == b->n) && (a->max == b->max) && (a->min == b->min) &&
			(a->mean == b->mean) && (a->median == b->median);
}

/** Writes value in the samples taken during the TIM2 interval [from, to) */
static void fillTimes(uint16_t *buffer, uint32_t from, uint32_t to, uint16_t value){
	st_window_t w = stWindowFromTimes(from, to, 0);
	uint32_t it;
	for (it = 0; it < w.count; it++)
		buffer[(w.first + it) & (MON_BUFFER_LENGTH - 1)] = value;
}

static void testStats(void){
	static uint16_t buffer[MON_BUFFER_LENGTH];
	static uint16_t scratch[MON_SCRATCH_LENGTH];
	const uint32_t mask = MON_BUFFER_LENGTH - 1;
	st_window_t w;
	st_phase_stats_t stats, reference;
	st_pulse_stats_t pulseStats;
	st_pulse_record_t pulse;
	char line[COMM_LINE_LENGTH];
	uint32_t n, it, base;

	/* windows: sample k is taken at (k + 1) * 2 us */
	w = stWindowFromTimes(100, 200, 0);
	CHECK((w.first == 49) && (w.count == 50));
	w = stWindowFromTimes(101, 200, 0);
	CHECK((w.first == 50) && (w.count == 49));
	w = stWindowFromTimes(100, 200, 4);
	CHECK((w.first == 51) && (w.count == 46));
	CHECK(stWindowFromTimes(200, 100, 0).count == 0);
	CHECK(stWindowFromTimes(100, 110, 6).count == 0);
	/* across the TIM2 wrap around: 32 us -> 16 samples, contiguous indexes */
	w = stWindowFromTimes(0xFFFFFFF0U, 0x10U, 0);
	CHECK(w.count == 16);
	CHECK(((w.first + 16) & mask) == (stWindowFromTimes(0x10U, 0x20U, 0).first & mask));

	/* statistics against a sorted reference, random data and sizes */
	srand(1234);
	for (it = 0; it < MON_BUFFER_LENGTH; it++)
		buffer[it] = (uint16_t)(rand() % 4096);
	for (n = 0; n < 600; n += 7){
		w.first = (uint32_t)rand();
		w.count = n;
		stPhaseStats(buffer, mask, w, scratch, &stats);
		referenceStats(buffer, mask, w, &reference);
		CHECK(sameStats(&stats, &reference));
	}
	/* many repeated values (median selection with duplicates) */
	for (it = 0; it < MON_BUFFER_LENGTH; it++)
		buffer[it] = (uint16_t)(rand() % 3);
	for (n = 1; n < 300; n += 13){
		w.first = (uint32_t)rand();
		w.count = n;
		stPhaseStats(buffer, mask, w, scratch, &stats);
		referenceStats(buffer, mask, w, &reference);
		CHECK(sameStats(&stats, &reference));
	}
	/* at most MON_SCRATCH_LENGTH samples are used */
	w.first = 0;
	w.count = MON_SCRATCH_LENGTH + 100;
	stPhaseStats(buffer, mask, w, scratch, &stats);
	CHECK(stats.n == MON_SCRATCH_LENGTH);

	/* synthetic biphasic pulse close to the TIM2 wrap around */
	base = 0xFFFFFF00U;
	pulse.frame = 7;
	pulse.slot = 1;
	pulse.ch = 3;
	pulse.positive = 0;
	pulse.biphasic = 1;
	pulse.amplitude = 150;
	pulse.tStart = base;
	pulse.tTrig = base + 50;
	pulse.tPhase1 = base + 60;
	pulse.tFlip = base + 160;
	pulse.tPhaseEnd = base + 260;
	pulse.tEnd = base + 310;
	for (it = 0; it < MON_BUFFER_LENGTH; it++)
		buffer[it] = 100;											// zero current
	fillTimes(buffer, pulse.tPhase1, pulse.tFlip, 2000);			// phase 1 plateau
	fillTimes(buffer, pulse.tFlip, pulse.tPhaseEnd, 1900);			// phase 2 plateau
	fillTimes(buffer, pulse.tPhase1 + 20, pulse.tPhase1 + 22, 2600);	// one spike
	fillTimes(buffer, pulse.tFlip + 40, pulse.tFlip + 42, 1500);		// one dip
	fillTimes(buffer, pulse.tFlip, pulse.tFlip + 2, 300);				// commutation edge, trimmed

	stPulseStats(buffer, mask, &pulse, 4, scratch, &pulseStats);
	CHECK(pulseStats.baseline == 100);
	CHECK(pulseStats.phases == 2);
	CHECK(pulseStats.phase[0].n == 46);					// 100 us = 50 samples - 2 x 4 us trim
	CHECK(pulseStats.phase[0].max == 2600);
	CHECK(pulseStats.phase[0].min == 2000);
	CHECK(pulseStats.phase[0].median == 2000);
	CHECK(pulseStats.phase[0].mean == (uint16_t)((45 * 2000 + 2600 + 23) / 46));
	CHECK(pulseStats.phase[1].max == 1900);
	CHECK(pulseStats.phase[1].min == 1500);				// the 300 edge is trimmed
	CHECK(pulseStats.phase[1].median == 1900);

	stFormatPulse(line, sizeof(line), &pulse, &pulseStats);
	snprintf(response, sizeof(response), "P 7 1 3 150 100 - 46 2600 %u 2000 2000 + 46 1900 %u 1900 1500",
			(unsigned)pulseStats.phase[0].mean, (unsigned)pulseStats.phase[1].mean);
	CHECK(strcmp(line, response) == 0);

	/* monophasic: a single phase over the whole pulse */
	pulse.biphasic = 0;
	pulse.positive = 1;
	stPulseStats(buffer, mask, &pulse, 0, scratch, &pulseStats);
	CHECK(pulseStats.phases == 1);
	CHECK(pulseStats.phase[0].n == 100);
	CHECK(pulseStats.phase[0].min == 300);
	stFormatPulse(line, sizeof(line), &pulse, &pulseStats);
	CHECK(strncmp(line, "P 7 1 3 150 100 + 100 2600 ", 27) == 0);
}


static void testEngine(void){
	long t0;
	int n;
	uint16_t dacHalf;

	memset(&fakeTim2, 0, sizeof(fakeTim2));
	odr = 0;
	stConfigureDefault();
	stEngineInit();
	CHECK(fakeTim2.PSC == 159);
	CHECK(monitorPrescaler == 159);					// same rate as TIM2
	CHECK(fakeTim2.CNT == 0);
	fakeTim2.CNT = 0xFFFFF000U;		// check the 32 bit wrap around

	/* Two slots per frame, 4 ms frame, 3 frames */
	EXPECT("SEQ 0 1", "OK");
	EXPECT("PERIOD 4000", "OK");
	EXPECT("CH 0 EN 1", "OK");
	EXPECT("CH 0 AMP 500", "OK");
	EXPECT("CH 1 EN 1", "OK");
	EXPECT("CH 1 AMP 1000", "OK");
	EXPECT("CH 1 POL ALT", "OK");
	resetTrace();
	pushedCount = 0;
	EXPECT("RUN 3", "OK");
	applyOutputs();
	CHECK(odr & ST_ARM_PIN);
	simulate(20000);

	/* one record per delivered pulse, with the times of the plan */
	CHECK(pushedCount == 6);
	for (n = 0; n < pushedCount; n++){
		const st_pulse_record_t *p = &pushed[n];
		CHECK(p->frame == (uint32_t)(n / 2));
		CHECK(p->slot == n % 2);
		CHECK(p->ch == n % 2);
		CHECK(p->amplitude == ((n % 2) ? 1000 : 500));
		CHECK(p->biphasic == 0);
		CHECK(p->tTrig - p->tStart == DEFAULT_SETTLE_US);
		CHECK(p->tPhase1 - p->tTrig == DEFAULT_DS8R_DELAY_US);
		CHECK(p->tPhaseEnd - p->tPhase1 == DEFAULT_PULSE_US);
		CHECK(p->tEnd - p->tStart == DEFAULT_SETTLE_US + DEFAULT_DS8R_DELAY_US + DEFAULT_PULSE_US + DEFAULT_TAIL_US);
		if (n > 0)
			CHECK(p->tStart - pushed[n - 1].tStart == 2000);
	}
	CHECK(pushed[1].positive && !pushed[3].positive && pushed[5].positive);	// CH1 alternates
	CHECK(pushed[0].positive && pushed[2].positive);
	checkInvariants();
	CHECK(stEngineIsRunning() == 0);
	CHECK(stEngineTakeDoneFlag() == 1);
	CHECK(stEngineTakeDoneFlag() == 0);
	CHECK(stEngineFrameCount() == 3);
	CHECK((odr & ST_PINS_ALL) == 0);					// safe state at the end
	CHECK(fakeDac1.DHR12R1 == 0);

	/* 6 pulses: CH0, CH1, CH0, CH1, CH0, CH1 */
	CHECK(risingEdge(ST_TRIG_PIN, 5) >= 0);
	CHECK(risingEdge(ST_TRIG_PIN, 6) < 0);

	t0 = risingEdge(ST_CH0_PIN, 0);
	dacHalf = stAmplitudeToDac(stGetConfig(), 500);
	CHECK(t0 >= 0);
	CHECK(dacAt(t0) == dacHalf);
	CHECK(stateAt(t0) & ST_POL_PIN);
	CHECK(risingEdge(ST_TRIG_PIN, 0) == t0 + DEFAULT_SETTLE_US);
	CHECK(fallingEdge(ST_TRIG_PIN, 0) == t0 + DEFAULT_SETTLE_US + DEFAULT_TRIGGER_US);
	CHECK(fallingEdge(ST_CH0_PIN, 0) == t0 + DEFAULT_SETTLE_US + DEFAULT_DS8R_DELAY_US + DEFAULT_PULSE_US + DEFAULT_TAIL_US);
	CHECK(dacAt(fallingEdge(ST_CH0_PIN, 0)) == 0);
	CHECK(risingEdge(ST_CH1_PIN, 0) == t0 + 2000);		// slot spacing
	CHECK(risingEdge(ST_CH0_PIN, 1) == t0 + 4000);		// frame period
	CHECK(dacAt(risingEdge(ST_CH1_PIN, 0)) == 4095);

	/* alternate polarity on CH1: +, -, + */
	CHECK(stateAt(risingEdge(ST_TRIG_PIN, 1)) & ST_POL_PIN);
	CHECK(stateAt(risingEdge(ST_TRIG_PIN, 3)) & ST_NPOL_PIN);
	CHECK(stateAt(risingEdge(ST_TRIG_PIN, 5)) & ST_POL_PIN);
	/* fixed polarity on CH0 */
	CHECK(stateAt(risingEdge(ST_TRIG_PIN, 2)) & ST_POL_PIN);

	/* Biphasic on CH0, CH1 disabled while running, continuous */
	EXPECT("CH 0 POL BIPH", "OK");
	EXPECT("CH 0 POL NEG", "OK");
	EXPECT("CH 0 POL BIPH", "OK");
	resetTrace();
	pushedCount = 0;
	EXPECT("RUN", "OK");
	simulate(5000);
	EXPECT("CH 1 EN 0", "OK");
	simulate(10000);
	EXPECT("STOP", "OK");
	applyOutputs();
	checkInvariants();
	CHECK((odr & ST_PINS_ALL) == 0);
	t0 = risingEdge(ST_CH0_PIN, 0);
	CHECK(stateAt(t0 + DEFAULT_SETTLE_US) & ST_POL_PIN);
	CHECK(fallingEdge(ST_POL_PIN, 0) == t0 + DEFAULT_SETTLE_US + DEFAULT_DS8R_DELAY_US + DEFAULT_PULSE_US / 2);
	CHECK(stateAt(t0 + DEFAULT_SETTLE_US + DEFAULT_DS8R_DELAY_US + DEFAULT_PULSE_US / 2) & ST_NPOL_PIN);
	CHECK(pushed[0].ch == 0 && pushed[0].biphasic && pushed[0].positive);
	CHECK(pushed[0].tFlip - pushed[0].tPhase1 == DEFAULT_PULSE_US / 2);
	CHECK(pushed[0].tPhaseEnd - pushed[0].tFlip == DEFAULT_PULSE_US / 2);
	/* CH1 stopped pulsing after being disabled */
	for (n = 0; risingEdge(ST_CH1_PIN, n) >= 0; n++)
		CHECK(risingEdge(ST_CH1_PIN, n) < (trace[0].t + 5000 + 2000));
	CHECK(n >= 1);

	/* Zero amplitude never triggers the DS8R */
	EXPECT("CH 0 AMP 0", "OK");
	EXPECT("CH 1 AMP 0", "OK");
	EXPECT("ENS 1 1", "OK");
	resetTrace();
	EXPECT("RUN 2", "OK");
	simulate(12000);
	CHECK(risingEdge(ST_TRIG_PIN, 0) < 0);
	CHECK(stEngineFrameCount() == 2);
}


int main(void){
	testModel();
	testProtocol();
	testEngine();
	testStats();
	if (failures){
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("All tests passed\n");
	return 0;
}
