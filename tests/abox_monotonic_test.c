#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../targets/stm32f105_ec800_v1/monotonic_stm32f105.c"
TIM_TypeDef test_timer;
RCC_TypeDef test_rcc = {0x400};
uint32_t uwTick = 17, test_mask;
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 36000000; }
void HAL_NVIC_SetPriority(int irq, unsigned p, unsigned s) { assert(irq == 28 && !p && !s); }
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(irq == 28); }
void HAL_NVIC_EnableIRQ(int irq) { assert(irq == 28); }
void Error_Handler(void) { abort(); }
static void advance(uint32_t ticks)
{
    uint32_t total = test_timer.CNT + ticks;
    assert(ticks < 65536); /* The proved operational bound: at most one wrap. */
    if (total >= 65536) test_timer.SR |= TIM_SR_UIF;
    test_timer.CNT = total & 65535;
}
int main(void)
{
    uint64_t before;
    assert(HAL_GetTick() == 17);
    ABoxStm32Monotonic_Init();
    assert(test_timer.PSC == 35999 && test_timer.ARR == 65535);
    advance(65530); before = ABoxStm32Monotonic_GetMs();
    test_mask = 1;
    advance(80); /* Flash erase spanning wrap, no interrupts for 40 ms. */
    assert(ABoxStm32Monotonic_GetMs() == before + 40 && test_mask == 1);
    before = ABoxStm32Monotonic_GetMs();
    TIM2_IRQHandler();
    assert(ABoxStm32Monotonic_GetMs() == before); /* Reader/ISR never double count. */
    advance(64000); /* 32 s IRQ blackout, still below 32.768 s. */
    assert(ABoxStm32Monotonic_GetMs() == before + 32000);
    test_mask = 0;
    for (unsigned i = 0; i < 5; ++i) {
        before = ABoxStm32Monotonic_GetMs(); advance(44);
        assert(ABoxStm32Monotonic_GetMs() == before + 22 && !test_mask);
    }
    g_high = 1ULL << 34; test_timer.CNT = 200; test_timer.SR = 0;
    assert(ABoxStm32Monotonic_GetMs() == 17 + (1ULL << 33) + 100);
    assert(HAL_GetTick() == 117); /* HAL wraps, UTC monotonic stays 64 bit. */
    for (uint32_t phase=0; phase<65536; ++phase) {
        static const uint32_t gaps[] = {80, 64000, 65535};
        for (unsigned i=0; i<3; ++i) {
            g_high = 100ULL * 65536; test_timer.CNT = phase; test_timer.SR = 0;
            advance(gaps[i]);
            assert(ABoxStm32Monotonic_GetMs() == 17 + (100ULL * 65536 + phase + gaps[i]) / 2);
        }
    }
    puts("actual TIM2 provider: wrap, pending UIF, IRQ mask, flash stalls, 64 bit PASS");
}
