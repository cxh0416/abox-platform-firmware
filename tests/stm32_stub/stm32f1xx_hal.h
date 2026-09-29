#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H
#include <stdint.h>
typedef struct { uint32_t ODR; uint32_t IDR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef gpio_a, gpio_b, gpio_c;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *gpio, uint16_t pin);
void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *config);
void mock_clock(unsigned port);
#define __HAL_RCC_GPIOA_CLK_ENABLE() mock_clock(0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() mock_clock(1)
#define __HAL_RCC_GPIOC_CLK_ENABLE() mock_clock(2)
#define __get_PRIMASK() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(key) ((void)(key))
#endif
