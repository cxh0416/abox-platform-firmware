#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
typedef struct { volatile uint32_t CR1, DIER, PSC, ARR, EGR, CNT, SR; } TIM_TypeDef;
typedef struct { uint32_t CFGR; } RCC_TypeDef;
extern TIM_TypeDef test_timer;
extern RCC_TypeDef test_rcc;
extern uint32_t uwTick, test_mask;
#define TIM2 (&test_timer)
#define RCC (&test_rcc)
#define RCC_CFGR_PPRE1 0x700U
#define TIM_SR_UIF 1U
#define TIM_EGR_UG 1U
#define TIM_DIER_UIE 1U
#define TIM_CR1_CEN 1U
#define TIM2_IRQn 28
#define __HAL_RCC_TIM2_CLK_ENABLE() ((void)0)
#define __get_PRIMASK() test_mask
#define __disable_irq() (test_mask = 1U)
#define __set_PRIMASK(x) (test_mask = (x))
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_NVIC_SetPriority(int irq, unsigned priority, unsigned sub);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_NVIC_EnableIRQ(int irq);
void Error_Handler(void);
#endif
