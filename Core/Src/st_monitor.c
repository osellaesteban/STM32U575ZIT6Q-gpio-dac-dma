/*
 * st_monitor.c
 *
 *  See st_monitor.h. ADC1 is programmed at register level because the ADC
 *  HAL is not part of the CubeMX project (ADC1 is not enabled in the .ioc).
 */
#include <stdio.h>
#include <string.h>

#include "st_monitor.h"
#include "st_comm.h"
#include "st_hw_config.h"

#define BUFFER_MASK			(MON_BUFFER_LENGTH - 1U)
#define QUEUE_LENGTH		16U			// power of 2
#define READY_MARGIN_US		(3U * MON_TS_US)	// conversion + DMA latency
#define OVERWRITE_MARGIN	64U			// samples
#define SYNC_CHECK_MS		10U
#define WAVE_VALUES_PER_LINE	24U		// "WD nnnn" + 24 * " vvvv" fits in COMM_LINE_LENGTH
#define INIT_TIMEOUT_MS		10U

typedef enum {
	WAVE_IDLE, WAVE_ARMED, WAVE_HEADER, WAVE_DATA
}st_wave_state_t;

/** Private variables */
static volatile uint16_t samples[MON_BUFFER_LENGTH];
static uint16_t scratch[MON_SCRATCH_LENGTH];
static uint16_t wave[MON_WAVE_MAX];

static DMA_HandleTypeDef hdmaAdc;
static DMA_NodeTypeDef adcNode;
static DMA_QListTypeDef adcQueue;

static uint8_t ready;
static uint8_t sampling;
static volatile uint8_t reports;
static uint32_t trim = DEFAULT_MON_TRIM_US;
static volatile uint32_t lost;
static uint8_t syncError;
static uint32_t lastSyncCheck;

static st_pulse_record_t queue[QUEUE_LENGTH];
static volatile uint32_t queueHead;		// written by the engine ISR
static volatile uint32_t queueTail;		// written by the main loop

static volatile st_wave_state_t waveState;
static volatile int16_t waveChannel;
static st_pulse_record_t wavePulse;
static uint32_t waveLength;
static uint32_t waveSent;


/** Private functions */

static uint8_t waitFlag(volatile uint32_t *reg, uint32_t mask, uint32_t value){
	uint32_t start = HAL_GetTick();
	while ((*reg & mask) != value){
		if ((HAL_GetTick() - start) > INIT_TIMEOUT_MS)
			return 0;
	}
	return 1;
}


static uint8_t adcInit(void){
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	gpio.Pin = ST_MON_ADC_PIN;
	gpio.Mode = GPIO_MODE_ANALOG;
	gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ST_MON_ADC_PORT, &gpio);

	/* Kernel clock: ADCDAC clock (HSI 16 MHz, selected in HAL_DAC_MspInit) */
	__HAL_RCC_ADC12_CLK_ENABLE();
	ADC12_COMMON->CCR &= ~ADC_CCR_PRESC;

	/* Exit deep power down, enable the internal regulator */
	ADC1->CR &= ~ADC_CR_DEEPPWD;
	ADC1->CR |= ADC_CR_ADVREGEN;
	if (!waitFlag(&ADC1->ISR, ADC_ISR_LDORDY, ADC_ISR_LDORDY))
		return 0;

	/* Offset calibration (ADEN = 0) */
	ADC1->CR = (ADC1->CR & ~ADC_CR_ADCALLIN) | ADC_CR_ADCAL;
	if (!waitFlag(&ADC1->CR, ADC_CR_ADCAL, 0))
		return 0;

	/* 12 bits (conversion ~1.1 us at 16 MHz, below the 2 us period), one
	 * conversion per TIM6 TRGO rising edge, circular DMA, overrun overwrites */
	ADC1->CFGR1 = ADC_CFGR1_RES_0
			| ADC_CFGR1_EXTEN_0
			| ADC_CFGR1_EXTSEL_3 | ADC_CFGR1_EXTSEL_2 | ADC_CFGR1_EXTSEL_0	// TIM6_TRGO
			| ADC_CFGR1_DMNGT_1 | ADC_CFGR1_DMNGT_0
			| ADC_CFGR1_OVRMOD;
	ADC1->CFGR2 = 0;
	ADC1->SMPR1 &= ~(ADC_SMPR1_SMP0 << (3U * ST_MON_ADC_CHANNEL));	// 5 cycles
	ADC1->PCSEL = (1UL << ST_MON_ADC_CHANNEL);
	ADC1->SQR1 = (ST_MON_ADC_CHANNEL << ADC_SQR1_SQ1_Pos);			// L = 0: one conversion

	ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
	ADC1->CR |= ADC_CR_ADEN;
	return waitFlag(&ADC1->ISR, ADC_ISR_ADRDY, ADC_ISR_ADRDY);
}


static uint8_t dmaInit(void){
	DMA_NodeConfTypeDef node = {0};

	hdmaAdc.Instance = ST_MON_DMA_CHANNEL;
	hdmaAdc.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
	hdmaAdc.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	hdmaAdc.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	hdmaAdc.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	hdmaAdc.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
	if (HAL_DMAEx_List_Init(&hdmaAdc) != HAL_OK)
		return 0;
	if (HAL_DMA_ConfigChannelAttributes(&hdmaAdc, DMA_CHANNEL_NPRIV) != HAL_OK)
		return 0;

	node.NodeType = DMA_GPDMA_LINEAR_NODE;
	node.Init.Request = GPDMA1_REQUEST_ADC1;
	node.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
	node.Init.Direction = DMA_PERIPH_TO_MEMORY;
	node.Init.SrcInc = DMA_SINC_FIXED;
	node.Init.DestInc = DMA_DINC_INCREMENTED;
	node.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
	node.Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
	node.Init.SrcBurstLength = 1;
	node.Init.DestBurstLength = 1;
	node.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
	node.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
	node.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
	node.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
	node.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
	node.SrcAddress = (uint32_t)&ADC1->DR;
	node.DstAddress = (uint32_t)samples;
	node.DataSize = MON_BUFFER_LENGTH * sizeof(samples[0]);

	if (HAL_DMAEx_List_BuildNode(&node, &adcNode) != HAL_OK)
		return 0;
	if (HAL_DMAEx_List_InsertNode_Tail(&adcQueue, &adcNode) != HAL_OK)
		return 0;
	if (HAL_DMAEx_List_SetCircularMode(&adcQueue) != HAL_OK)
		return 0;
	if (HAL_DMAEx_List_LinkQ(&hdmaAdc, &adcQueue) != HAL_OK)
		return 0;
	return HAL_DMAEx_List_Start(&hdmaAdc) == HAL_OK;
}


static void timerInit(uint32_t prescaler){
	__HAL_RCC_TIM6_CLK_ENABLE();
	TIM6->CR1 = 0;
	TIM6->PSC = prescaler;
	TIM6->ARR = MON_TS_US - 1U;
	TIM6->CR2 = TIM_CR2_MMS_1;		// TRGO = update
	TIM6->EGR = TIM_EGR_UG;			// load the prescaler (ADC not started yet)
	TIM6->SR = 0;
	TIM6->CNT = 0;
}


/**
 * The sample being written must match the one expected from TIM2 time,
 * otherwise conversions were lost and the time to sample mapping is wrong.
 */
static void checkSync(void){
	uint32_t now, remaining, position, expected, diff;

	if (!sampling || ((HAL_GetTick() - lastSyncCheck) < SYNC_CHECK_MS))
		return;
	lastSyncCheck = HAL_GetTick();
	now = TIM2->CNT;
	remaining = (ST_MON_DMA_CHANNEL->CBR1 & DMA_CBR1_BNDT) / sizeof(samples[0]);
	position = (MON_BUFFER_LENGTH - remaining) & BUFFER_MASK;	// next sample to be written
	expected = (now >> MON_TS_SHIFT) & BUFFER_MASK;			// samples triggered so far
	diff = (position - expected) & BUFFER_MASK;
	if ((diff > 2U) && (diff < (BUFFER_MASK - 2U)))
		syncError = 1;
}


static uint8_t samplesAvailable(const st_pulse_record_t *pulse, uint32_t now){
	return (now - pulse->tStart) < ((MON_BUFFER_LENGTH - OVERWRITE_MARGIN) * MON_TS_US);
}


static void snapshotWave(const st_pulse_record_t *pulse){
	st_window_t window = stWindowFromTimes(pulse->tStart, pulse->tEnd, 0);
	uint32_t it;

	waveLength = (window.count > MON_WAVE_MAX) ? MON_WAVE_MAX : window.count;
	for (it = 0; it < waveLength; it++)
		wave[it] = samples[(window.first + it) & BUFFER_MASK];
	wavePulse = *pulse;
	waveSent = 0;
	waveState = WAVE_HEADER;
}


/**
 * WH <frame> <slot> <ch> <amp> <+|-> <biphasic> <ts_us> <n> <i_trig> <i_phase1> <i_flip> <i_end>
 * WD <index> <v> <v> ...
 * WE
 * Indexes are relative to the first sample (taken when the bridge closes).
 */
static void streamWave(void){
	char line[COMM_LINE_LENGTH];
	int written;
	uint32_t it, last;

	while ((waveState == WAVE_HEADER) || (waveState == WAVE_DATA)){
		if (stCommTxFree() < sizeof(line))
			return;		// continue on the next call

		if (waveState == WAVE_HEADER){
			const st_pulse_record_t *p = &wavePulse;
			snprintf(line, sizeof(line), "WH %lu %u %u %u %c %u %lu %lu %lu %lu %lu %lu",
					(unsigned long)p->frame, (unsigned)p->slot, (unsigned)p->ch,
					(unsigned)p->amplitude, p->positive ? '+' : '-', (unsigned)p->biphasic,
					(unsigned long)MON_TS_US, (unsigned long)waveLength,
					(unsigned long)stWindowFromTimes(p->tStart, p->tTrig, 0).count,
					(unsigned long)stWindowFromTimes(p->tStart, p->tPhase1, 0).count,
					(unsigned long)(p->biphasic ? stWindowFromTimes(p->tStart, p->tFlip, 0).count : 0U),
					(unsigned long)stWindowFromTimes(p->tStart, p->tPhaseEnd, 0).count);
			stCommWriteLine(line);
			waveState = WAVE_DATA;
		}
		else if (waveSent < waveLength){
			written = snprintf(line, sizeof(line), "WD %lu", (unsigned long)waveSent);
			last = waveSent + WAVE_VALUES_PER_LINE;
			if (last > waveLength)
				last = waveLength;
			for (it = waveSent; (it < last) && (written > 0) && (written < (int)sizeof(line)); it++)
				written += snprintf(&line[written], sizeof(line) - (size_t)written, " %u", (unsigned)wave[it]);
			stCommWriteLine(line);
			waveSent = last;
		}
		else{
			stCommWriteLine("WE");
			waveState = WAVE_IDLE;
		}
	}
}


/** Public functions */

void stMonitorInit(uint32_t prescaler){
	ready = 0;
	sampling = 0;
	timerInit(prescaler);
	if (!adcInit() || !dmaInit())
		return;		// the stimulator works without the monitor (STATUS reports MSYNC=1)
	ADC1->CR |= ADC_CR_ADSTART;	// waits for the first TIM6 trigger
	ready = 1;
}


void stMonitorStartSampling(void){
	if (!ready)
		return;
	TIM6->CR1 = TIM_CR1_CEN;
	sampling = 1;
	lastSyncCheck = HAL_GetTick();
}


void stMonitorPushPulse(const st_pulse_record_t *pulse){
	uint32_t next;
	if (!sampling || (!reports && (waveState != WAVE_ARMED)))
		return;
	next = (queueHead + 1U) & (QUEUE_LENGTH - 1U);
	if (next == queueTail){
		lost++;
		return;
	}
	queue[queueHead] = *pulse;
	queueHead = next;
}


void stMonitorProcess(void){
	char line[COMM_LINE_LENGTH];
	st_pulse_stats_t stats;

	checkSync();
	streamWave();

	while (queueTail != queueHead){
		const st_pulse_record_t *pulse = &queue[queueTail];
		uint32_t now = TIM2->CNT;

		if ((int32_t)(now - (pulse->tEnd + READY_MARGIN_US)) < 0)
			break;		// last samples not converted yet

		if (!samplesAvailable(pulse, now))
			lost++;
		else{
			if ((waveState == WAVE_ARMED) && ((waveChannel < 0) || (waveChannel == pulse->ch)))
				snapshotWave(pulse);
			if (reports){
				stPulseStats(samples, BUFFER_MASK, pulse, trim, scratch, &stats);
				if (samplesAvailable(pulse, TIM2->CNT)){
					stFormatPulse(line, sizeof(line), pulse, &stats);
					stCommTryWriteLine(line);
				}
				else
					lost++;
			}
		}
		queueTail = (queueTail + 1U) & (QUEUE_LENGTH - 1U);
	}
}


void stMonitorSetReports(uint8_t enabled){
	reports = enabled ? 1 : 0;
}


uint8_t stMonitorGetReports(void){
	return reports;
}


st_status_t stMonitorSetTrim(uint32_t value){
	if (value > MON_TRIM_MAX_US)
		return ST_ERR_RANGE;
	trim = value;
	return ST_OK;
}


uint32_t stMonitorGetTrim(void){
	return trim;
}


st_status_t stMonitorRequestWave(int16_t ch){
	if ((ch >= CHAN_LENGTH) || (ch < -1))
		return ST_ERR_CHANNEL;
	if (!ready)
		return ST_ERR_UNAVAILABLE;
	if (waveState != WAVE_IDLE)
		return ST_ERR_BUSY;
	waveChannel = ch;
	waveState = WAVE_ARMED;
	return ST_OK;
}


uint32_t stMonitorLost(void){
	return lost;
}


uint8_t stMonitorSyncError(void){
	return (!ready || syncError) ? 1 : 0;
}
