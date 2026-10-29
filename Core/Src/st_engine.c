/*
 * st_engine.c
 *
 *  Stimulation engine, see st_engine.h.
 *
 *  Event sequence of a slot (offsets from st_slot_plan_t):
 *    EV_SLOT_START  DAC = amplitude, channel select + polarity lines set
 *    EV_TRIG_ON     DS8R trigger high
 *    EV_TRIG_OFF    DS8R trigger low
 *    EV_FLIP        polarity lines swapped (biphasic channels only)
 *    EV_GATE_OFF    channel select lines low (electrodes shorted), DAC = 0
 *  Disabled channels or channels with zero amplitude skip the whole slot,
 *  so the DS8R is never triggered for them (the DAC buffered output is not
 *  exactly 0 V, a zero amplitude must not rely on it).
 */
#include <string.h>

#include "st_engine.h"
#include "st_stimulator.h"
#include "st_monitor.h"
#include "st_hw_config.h"

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim2;

typedef enum {
	EV_SLOT_START, EV_TRIG_ON, EV_TRIG_OFF, EV_FLIP, EV_GATE_OFF
}st_event_t;

#define BSRR_SET(pins)		((uint32_t)(pins))
#define BSRR_RESET(pins)	((uint32_t)(pins) << 16)
#define BSRR_POSITIVE		(BSRR_SET(ST_POL_PIN) | BSRR_RESET(ST_NPOL_PIN))
#define BSRR_NEGATIVE		(BSRR_SET(ST_NPOL_PIN) | BSRR_RESET(ST_POL_PIN))
// bridges disabled (high impedance), trigger low
#define BSRR_SAFE			BSRR_RESET(ST_PINS_ALL)
// bridges enabled with both low sides on (electrodes shorted), trigger low
#define BSRR_IDLE			(BSRR_SET(ST_ARM_PIN) | BSRR_RESET(ST_CH_PINS_ALL | ST_TRIG_PIN))

/** Private variables */
static const uint16_t chPins[CHAN_LENGTH] = {ST_CH0_PIN, ST_CH1_PIN, ST_CH2_PIN, ST_CH3_PIN,
		ST_CH4_PIN, ST_CH5_PIN, ST_CH6_PIN, ST_CH7_PIN};

static st_config_t active;				// configuration copied at start
static st_slot_plan_t plan;
static uint32_t slotOffset[MAX_SLOTS];	// slot start relative to the frame start
static volatile uint8_t running;
static volatile uint8_t doneFlag;
static volatile uint32_t frameCount;
static uint32_t frameTarget;
static uint32_t frameStart;				// TIM2 count at the frame start
static uint32_t slotStart;				// TIM2 count at the current slot start
static uint8_t slot;
static st_event_t nextEvent;
static uint8_t slotBiphasic;
static uint8_t slotPositive;
static uint8_t altPhase[CHAN_LENGTH];	// alternate polarity state of each channel
static st_pulse_record_t pulse;			// pulse in progress, reported to the monitor


/** Private functions */

static inline void dacWrite(uint16_t counts){
	// DAC trigger disabled: DHR is copied to the output on the next APB clock
	DAC1->DHR12R1 = counts;
}


static void finish(void){
	TIM2->DIER &= ~TIM_DIER_CC1IE;
	running = 0;
	ST_PORT->BSRR = BSRR_SAFE;
	dacWrite(0);
}


/**
 * Moves to the next slot. Returns the time of the next event.
 */
static uint32_t endSlot(void){
	slot++;
	if (slot >= active.stSeqLength){
		slot = 0;
		frameCount++;
		frameStart += active.stPeriod;
		if ((frameTarget != 0) && (frameCount >= frameTarget)){
			finish();
			doneFlag = 1;
			return 0;
		}
	}
	slotStart = frameStart + slotOffset[slot];
	nextEvent = EV_SLOT_START;
	return slotStart;
}


/**
 * Executes the pending event. Returns the time of the next event.
 */
static uint32_t processEvent(void){
	switch (nextEvent){
	case EV_SLOT_START:{
		uint8_t ch = active.stSequence[slot];
		const st_channel_t *channel = stGetChannel(ch);
		uint16_t amplitude = channel->stAmplitude;
		uint8_t polarity = channel->stPolarity;

		if ((channel->stActiveState != st_enabled) || (amplitude == 0))
			return endSlot();

		if (polarity == st_pol_negative)
			slotPositive = 0;
		else if (polarity == st_pol_alternate){
			slotPositive = !altPhase[ch];
			altPhase[ch] = slotPositive;
		}
		else
			slotPositive = 1;
		slotBiphasic = (polarity == st_pol_biphasic);

		dacWrite(stAmplitudeToDac(&active, amplitude));
		ST_PORT->BSRR = BSRR_SET(chPins[ch]) | (slotPositive ? BSRR_POSITIVE : BSRR_NEGATIVE);

		pulse.frame = frameCount;
		pulse.slot = slot;
		pulse.ch = ch;
		pulse.positive = slotPositive;
		pulse.biphasic = slotBiphasic;
		pulse.amplitude = amplitude;
		pulse.tStart = slotStart;
		pulse.tTrig = slotStart + plan.trigOn;
		pulse.tPhase1 = pulse.tTrig + active.stDs8rDelay;
		pulse.tFlip = slotStart + plan.flip;
		pulse.tPhaseEnd = pulse.tPhase1 + active.stPulse;
		pulse.tEnd = slotStart + plan.gateOff;
		nextEvent = EV_TRIG_ON;
		return slotStart + plan.trigOn;
	}
	case EV_TRIG_ON:
		ST_PORT->BSRR = BSRR_SET(ST_TRIG_PIN);
		nextEvent = EV_TRIG_OFF;
		return slotStart + plan.trigOff;

	case EV_TRIG_OFF:
		ST_PORT->BSRR = BSRR_RESET(ST_TRIG_PIN);
		if (slotBiphasic){
			nextEvent = EV_FLIP;
			return slotStart + plan.flip;
		}
		nextEvent = EV_GATE_OFF;
		return slotStart + plan.gateOff;

	case EV_FLIP:
		// both half bridges commute with the same write
		ST_PORT->BSRR = slotPositive ? BSRR_NEGATIVE : BSRR_POSITIVE;
		nextEvent = EV_GATE_OFF;
		return slotStart + plan.gateOff;

	case EV_GATE_OFF:
	default:
		ST_PORT->BSRR = BSRR_RESET(ST_CH_PINS_ALL);
		dacWrite(0);
		stMonitorPushPulse(&pulse);
		return endSlot();
	}
}


/** Public functions */

void stEngineInit(void){
	GPIO_InitTypeDef gpio = {0};
	DAC_ChannelConfTypeDef dac = {0};
	RCC_ClkInitTypeDef clk;
	uint32_t latency;
	uint32_t timClock;

	/* Multiplexer lines, safe state before switching them to outputs */
	ST_PORT_CLK_ENABLE();
	ST_PORT->BSRR = BSRR_SAFE;
	gpio.Pin = ST_PINS_ALL;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_PULLDOWN;
	gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(ST_PORT, &gpio);

	/* VDDA is isolated after reset on the STM32U5: required by the DAC and the ADC */
	HAL_PWREx_EnableVddA();

	/* DAC updated by software (no trigger). The HF interface mode depends on
	 * HCLK, so it is selected automatically. */
	HAL_DAC_Stop(&hdac1, ST_DAC_CHANNEL);
	dac.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
	dac.DAC_DMADoubleDataMode = DISABLE;
	dac.DAC_SignedFormat = DISABLE;
	dac.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
	dac.DAC_Trigger = DAC_TRIGGER_NONE;
	dac.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	dac.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
	dac.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
	if (HAL_DAC_ConfigChannel(&hdac1, &dac, ST_DAC_CHANNEL) != HAL_OK)
		Error_Handler();
	if (HAL_DACEx_SelfCalibrate(&hdac1, &dac, ST_DAC_CHANNEL) != HAL_OK)
		Error_Handler();
	if (HAL_DAC_SetValue(&hdac1, ST_DAC_CHANNEL, DAC_ALIGN_12B_R, 0) != HAL_OK)
		Error_Handler();
	if (HAL_DAC_Start(&hdac1, ST_DAC_CHANNEL) != HAL_OK)
		Error_Handler();

	/* TIM2: free running 32 bit counter with 1 us tick, CC1 as event timer */
	HAL_RCC_GetClockConfig(&clk, &latency);
	timClock = HAL_RCC_GetPCLK1Freq();
	if (clk.APB1CLKDivider != RCC_HCLK_DIV1)
		timClock *= 2U;
	if ((timClock % 1000000U) != 0U)
		Error_Handler();	// the timer clock must be a multiple of 1 MHz

	HAL_TIM_Base_Stop(&htim2);
	TIM2->CR1 = 0;
	TIM2->DIER = 0;
	TIM2->PSC = (timClock / 1000000U) - 1U;
	TIM2->ARR = 0xFFFFFFFFU;
	TIM2->CCMR1 = 0;		// CC1 output compare, frozen (only generates the flag)
	TIM2->CCER = 0;
	TIM2->EGR = TIM_EGR_UG;	// load the prescaler
	TIM2->SR = 0;
	TIM2->CNT = 0;

	/* The current monitor samples with a timer that runs at the same rate as
	 * TIM2 and starts with it, so each sample has a known TIM2 time */
	stMonitorInit(TIM2->PSC);
	TIM2->CR1 = TIM_CR1_CEN;
	stMonitorStartSampling();

	HAL_NVIC_SetPriority(TIM2_IRQn, ST_ENGINE_IRQ_PRIO, 0);
	HAL_NVIC_EnableIRQ(TIM2_IRQn);
}


st_status_t stEngineStart(uint32_t frames){
	st_status_t res;
	uint8_t it;

	if (running)
		return ST_ERR_BUSY;
	res = stValidate();
	if (res != ST_OK)
		return res;

	active = *stGetConfig();
	stComputeSlotPlan(&active, &plan);
	for (it = 0; it < active.stSeqLength; it++)
		slotOffset[it] = (uint32_t)(((uint64_t)it * active.stPeriod) / active.stSeqLength);
	memset(altPhase, 0, sizeof(altPhase));
	frameCount = 0;
	frameTarget = frames;
	doneFlag = 0;
	slot = 0;
	nextEvent = EV_SLOT_START;

	/* Arm the bridges and give time to charge the bootstrap capacitors */
	dacWrite(0);
	ST_PORT->BSRR = BSRR_IDLE;
	frameStart = TIM2->CNT + ARM_PRECHARGE_US;
	slotStart = frameStart;
	TIM2->CCR1 = slotStart;
	TIM2->SR = ~TIM_SR_CC1IF;
	running = 1;
	TIM2->DIER |= TIM_DIER_CC1IE;
	return ST_OK;
}


void stEngineStop(void){
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	finish();
	TIM2->SR = ~TIM_SR_CC1IF;
	__set_PRIMASK(primask);
}


uint8_t stEngineIsRunning(void){
	return running;
}


uint32_t stEngineFrameCount(void){
	return frameCount;
}


uint8_t stEngineTakeDoneFlag(void){
	uint8_t flag = doneFlag;
	doneFlag = 0;
	return flag;
}


void stEngineEmergencyStop(void){
	running = 0;
	if ((RCC->APB1ENR1 & RCC_APB1ENR1_TIM2EN) != 0U)
		TIM2->DIER = 0;
	if (ST_PORT_CLK_IS_ENABLED())
		ST_PORT->BSRR = BSRR_SAFE;
	if ((RCC->AHB3ENR & RCC_AHB3ENR_DAC1EN) != 0U)
		DAC1->DHR12R1 = 0;
}


/**
 * TIM2 compare interrupt. Not generated by CubeMX: keep the TIM2 global
 * interrupt disabled in the .ioc or this handler will be duplicated.
 */
void TIM2_IRQHandler(void){
	uint32_t target;

	if ((TIM2->SR & TIM_SR_CC1IF) == 0U)
		return;
	TIM2->SR = ~TIM_SR_CC1IF;
	if (!running)
		return;

	for (;;){
		target = processEvent();
		if (!running)
			break;
		TIM2->CCR1 = target;
		// discard a match of the previous compare value and check that the
		// next event is still in the future, otherwise execute it now
		TIM2->SR = ~TIM_SR_CC1IF;
		if ((int32_t)(target - TIM2->CNT) > 0)
			break;
	}
}
