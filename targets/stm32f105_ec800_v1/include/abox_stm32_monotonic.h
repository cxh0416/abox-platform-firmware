#ifndef ABOX_STM32_MONOTONIC_H
#define ABOX_STM32_MONOTONIC_H
#include <stdint.h>
/* App opt-in only; initialize after RCC setup, before any Flash writes.
 * TIM2 is reserved. No STOP/clock changes/debug freeze while running.
 * UIF must be consumed at least once every 32.768 s (not an event counter). */
void ABoxStm32Monotonic_Init(void);
uint64_t ABoxStm32Monotonic_GetMs(void);
#endif
