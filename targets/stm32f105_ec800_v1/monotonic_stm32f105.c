#include "abox_stm32_monotonic.h"
#include "main.h"

static volatile uint64_t g_high;
static uint32_t g_boot_ms;
static volatile uint8_t g_ready;

void ABoxStm32Monotonic_Init(void)
{
    uint32_t clock, divider;
    if (g_ready) return;
    clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) clock *= 2U;
    divider = clock / 2000U;
    if (clock % 2000U || !divider || divider > 65536U) Error_Handler();
    g_boot_ms = uwTick;
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0U;
    TIM2->DIER = 0U;
    TIM2->PSC = divider - 1U;
    TIM2->ARR = 65535U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CNT = 0U;
    TIM2->SR = 0U;
    g_high = 0U;
    HAL_NVIC_SetPriority(TIM2_IRQn, 0U, 0U);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->DIER = TIM_DIER_UIE;
    g_ready = 1U;
    TIM2->CR1 = TIM_CR1_CEN;
}

uint64_t ABoxStm32Monotonic_GetMs(void)
{
    uint32_t mask, before, counter, after;
    uint64_t ticks;
    if (!g_ready) return uwTick;
    mask = __get_PRIMASK();
    __disable_irq();
    before = TIM2->SR;
    counter = TIM2->CNT;
    after = TIM2->SR;
    if ((before | after) & TIM_SR_UIF) {
        /* If overflow raced the CNT read, read again after detecting UIF.
         * Reader and ISR consume the same flag under the same PRIMASK guard. */
        counter = TIM2->CNT;
        TIM2->SR = (uint32_t)~TIM_SR_UIF;
        g_high += 65536ULL;
    }
    ticks = g_high + counter;
    __set_PRIMASK(mask);
    return g_boot_ms + ticks / 2U;
}

void TIM2_IRQHandler(void)
{
    (void)ABoxStm32Monotonic_GetMs();
}

/* Preserve HAL's uint32 wrap arithmetic and early HAL/RCC initialization.
 * HAL_SuspendTick only masks the legacy TIM1 IRQ; it never freezes monotonic. */
uint32_t HAL_GetTick(void)
{
    return (uint32_t)ABoxStm32Monotonic_GetMs();
}
