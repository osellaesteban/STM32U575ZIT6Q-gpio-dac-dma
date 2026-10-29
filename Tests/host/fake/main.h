/*
 * Fake main.h to compile st_engine.c / st_comm.c on a PC. It replaces the
 * HAL and the registers used by the engine with plain variables.
 */
#ifndef FAKE_MAIN_H
#define FAKE_MAIN_H

#include <stdint.h>
#include <stdio.h>

typedef struct { volatile uint32_t BSRR; uint32_t ODR; } GPIO_TypeDef;
typedef struct { volatile uint32_t CR1, DIER, SR, EGR, CCMR1, CCER, CNT, PSC, ARR, CCR1; } TIM_TypeDef;
typedef struct { volatile uint32_t DHR12R1; } DAC_TypeDef;
typedef struct { uint32_t APB1ENR1, AHB2ENR1, AHB3ENR; } RCC_TypeDef;

extern GPIO_TypeDef fakeGpioE;
extern TIM_TypeDef fakeTim2;
extern DAC_TypeDef fakeDac1;
extern RCC_TypeDef fakeRcc;
#define GPIOE	(&fakeGpioE)
#define TIM2	(&fakeTim2)
#define DAC1	(&fakeDac1)
#define RCC		(&fakeRcc)

#define GPIO_PIN_0	0x0001U
#define GPIO_PIN_2	0x0004U
#define GPIO_PIN_3	0x0008U
#define GPIO_PIN_4	0x0010U
#define GPIO_PIN_5	0x0020U
#define GPIO_PIN_6	0x0040U
#define GPIO_PIN_7	0x0080U
#define GPIO_PIN_8	0x0100U
#define GPIO_PIN_9	0x0200U
#define GPIO_PIN_10	0x0400U
#define GPIO_PIN_11	0x0800U
#define GPIO_PIN_12	0x1000U

#define TIM_SR_CC1IF	0x2U
#define TIM_DIER_CC1IE	0x2U
#define TIM_EGR_UG		0x1U
#define TIM_CR1_CEN		0x1U
#define RCC_APB1ENR1_TIM2EN	0x1U
#define RCC_AHB2ENR1_GPIOEEN 0x10U
#define RCC_AHB3ENR_DAC1EN	0x40U
#define __HAL_RCC_GPIOE_CLK_ENABLE()	(RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOEEN)

/* HAL types and functions used by st_engine.c */
typedef enum { HAL_OK = 0, HAL_ERROR } HAL_StatusTypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t DAC_HighFrequency, DAC_DMADoubleDataMode, DAC_SignedFormat, DAC_SampleAndHold,
	DAC_Trigger, DAC_OutputBuffer, DAC_ConnectOnChipPeripheral, DAC_UserTrimming; } DAC_ChannelConfTypeDef;
typedef struct { uint32_t APB1CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { int dummy; } DAC_HandleTypeDef;
typedef struct { int dummy; } TIM_HandleTypeDef;
enum { GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN, GPIO_SPEED_FREQ_MEDIUM, DISABLE, DAC_SAMPLEANDHOLD_DISABLE,
	DAC_TRIGGER_NONE, DAC_OUTPUTBUFFER_ENABLE, DAC_CHIPCONNECT_EXTERNAL, DAC_TRIMMING_FACTORY,
	DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC, DAC_CHANNEL_1, DAC_ALIGN_12B_R, RCC_HCLK_DIV1, TIM2_IRQn };

static inline void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *i){ (void)p; (void)i; }
static inline HAL_StatusTypeDef HAL_DAC_Stop(DAC_HandleTypeDef *h, uint32_t c){ (void)h; (void)c; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_DAC_Start(DAC_HandleTypeDef *h, uint32_t c){ (void)h; (void)c; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_DAC_ConfigChannel(DAC_HandleTypeDef *h, DAC_ChannelConfTypeDef *s, uint32_t c){ (void)h; (void)s; (void)c; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_DACEx_SelfCalibrate(DAC_HandleTypeDef *h, DAC_ChannelConfTypeDef *s, uint32_t c){ (void)h; (void)s; (void)c; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_DAC_SetValue(DAC_HandleTypeDef *h, uint32_t c, uint32_t a, uint32_t v){ (void)h; (void)c; (void)a; (void)v; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef *h){ (void)h; return HAL_OK; }
static inline void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *c, uint32_t *l){ c->APB1CLKDivider = RCC_HCLK_DIV1; *l = 4; }
static inline uint32_t HAL_RCC_GetPCLK1Freq(void){ return 160000000U; }
static inline void HAL_NVIC_SetPriority(int irq, uint32_t p, uint32_t s){ (void)irq; (void)p; (void)s; }
static inline void HAL_NVIC_EnableIRQ(int irq){ (void)irq; }
static inline uint32_t __get_PRIMASK(void){ return 0; }
static inline void __disable_irq(void){ }
static inline void __set_PRIMASK(uint32_t m){ (void)m; }
void Error_Handler(void);

#endif
