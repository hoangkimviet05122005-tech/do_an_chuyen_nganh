/* Lay mau ECG: TIM3 250 Hz -> ADC1 (PA0) -> FIFO */
#include "main.h"
#include "ecg_config.h"
#include "ecg_sampling.h"

extern ADC_HandleTypeDef hadc1;

static volatile uint16_t fifo[FIFO_SIZE];
static volatile uint16_t fHead = 0, fTail = 0;

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = (uint16_t)~TIM_SR_UIF;

        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK) {
            uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
            uint16_t next = (uint16_t)((fHead + 1) % FIFO_SIZE);
            if (next != fTail) {
                fifo[fHead] = v;
                fHead = next;
            }
        }
    }
}

static void timerStart(void)
{
    uint32_t tclk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) tclk *= 2U;

    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM3->CR1  = 0;
    TIM3->PSC  = (uint16_t)(tclk / 10000UL - 1UL);
    TIM3->ARR  = (uint16_t)(10000UL / FS_HZ - 1UL);
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1  = TIM_CR1_CEN;
}

void Sampling_Init(void)
{
    ADC_ChannelConfTypeDef adcCh = {0};

    adcCh.Channel      = ADC_CHANNEL_0;
    adcCh.Rank         = ADC_REGULAR_RANK_1;
    adcCh.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &adcCh);
    HAL_ADCEx_Calibration_Start(&hadc1);

    fHead = fTail = 0;
    timerStart();
}

bool Sampling_Pop(uint16_t *v)
{
    if (fTail == fHead) return false;
    *v = fifo[fTail];
    fTail = (uint16_t)((fTail + 1) % FIFO_SIZE);
    return true;
}

void Sampling_Flush(void)
{
    fTail = fHead;
}
