/*
 * Host tests of the stimulator model, the command protocol and the engine
 * (st_engine.c is compiled against fake registers, see fake/main.h, and
 * TIM2 is simulated with a 1 us step).
 *
 * Build and run with run_tests.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st_stimulator.h"
#include "st_protocol.h"
#include "st_engine.h"
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
}


static void testEngine(void){
	long t0;
	int n;
	uint16_t dacHalf;

	memset(&fakeTim2, 0, sizeof(fakeTim2));
	fakeTim2.CNT = 0xFFFFF000U;		// also check the 32 bit wrap around
	odr = 0;
	stConfigureDefault();
	stEngineInit();
	CHECK(fakeTim2.PSC == 159);

	/* Two slots per frame, 4 ms frame, 3 frames */
	EXPECT("SEQ 0 1", "OK");
	EXPECT("PERIOD 4000", "OK");
	EXPECT("CH 0 EN 1", "OK");
	EXPECT("CH 0 AMP 500", "OK");
	EXPECT("CH 1 EN 1", "OK");
	EXPECT("CH 1 AMP 1000", "OK");
	EXPECT("CH 1 POL ALT", "OK");
	resetTrace();
	EXPECT("RUN 3", "OK");
	applyOutputs();
	CHECK(odr & ST_ARM_PIN);
	simulate(20000);
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
	if (failures){
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("All tests passed\n");
	return 0;
}
