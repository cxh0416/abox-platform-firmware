#include "abox_board_io_stm32f105.h"
#include "stm32f1xx_hal.h"

#include <assert.h>

GPIO_TypeDef gpio_a, gpio_b, gpio_c;
static uint32_t output_config[3], input_config[3], write_mask[3];
static unsigned clocks[3];

static unsigned index_of(GPIO_TypeDef *gpio)
{
    if (gpio == GPIOA) return 0;
    if (gpio == GPIOB) return 1;
    assert(gpio == GPIOC);
    return 2;
}
void mock_clock(unsigned port) { ++clocks[port]; }
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint16_t pin, GPIO_PinState state)
{
    unsigned i = index_of(gpio);
    write_mask[i] |= pin;
    if (state == GPIO_PIN_SET) gpio->ODR |= pin;
    else gpio->ODR &= (uint32_t)~pin;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *gpio, uint16_t pin)
{
    return (gpio->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *config)
{
    unsigned i = index_of(gpio);
    if (config->Mode == GPIO_MODE_OUTPUT_PP) {
        assert(!(gpio->ODR & config->Pin)); /* low before output mode */
        output_config[i] |= config->Pin;
    } else input_config[i] |= config->Pin;
}
int main(void)
{
    const ABoxBoardIoPort *port = ABoxBoardIoStm32_Port();
    ABoxBoardIo io;
    uint8_t level;
    unsigned i;
    gpio_a.ODR = 7U;
    gpio_b.ODR = 7U;
    gpio_c.ODR = 16U;
    /* LED-only initialization does not even clock/configure A or C. */
    assert(ABoxBoardIo_LedsInit(&io, port, 3U) == ABOX_BOARD_IO_OK);
    assert(gpio_a.ODR == 7U && gpio_c.ODR == 16U && gpio_b.ODR == 4U);
    assert(clocks[0] == 0 && clocks[2] == 0);
    assert(output_config[0] == 0 && output_config[2] == 0 && output_config[1] == 3U);
    assert(input_config[0] == 0 && input_config[1] == 0 && input_config[2] == 0);
    ABoxBoardIo_Poll(&io, 1000U);
    assert(gpio_a.ODR == 7U && gpio_c.ODR == 16U && gpio_b.ODR == 4U);
    assert(ABoxBoardIo_Init(&io, port, 0x0fU) == ABOX_BOARD_IO_OK);
    assert(output_config[0] == 0x07U && output_config[1] == 0x07U && output_config[2] == 0x10U);
    assert(input_config[0] == 0xf0U && input_config[2] == 0x0fU);
    assert(write_mask[0] == 0x07U && write_mask[1] == 0x07U && write_mask[2] == 0x10U);
    for (i = 0; i < 3; ++i) assert(clocks[i] != 0);
    for (i = 0; i < 4; ++i) {
        assert(ABoxBoardIo_OutputSet(&io, (ABoxBoardIoOutput)i, 1) == ABOX_BOARD_IO_OK);
        assert(ABoxBoardIo_OutputGet(&io, (ABoxBoardIoOutput)i, &level) == ABOX_BOARD_IO_OK && level);
    }
    assert(gpio_a.ODR == 0x07U && gpio_c.ODR == 0x10U);
    for (i = 0; i < 8; ++i) {
        static const uint8_t expected_pin[8] = {3, 2, 1, 0, 7, 6, 5, 4};
        gpio_c.IDR = i < 4 ? 1U << expected_pin[i] : 0U;
        gpio_a.IDR = i >= 4 ? 1U << expected_pin[i] : 0U;
        assert(port->read_input(0, (ABoxBoardIoInput)i, &level) && level == 1U);
    }
    gpio_c.IDR = 0x0fU;
    gpio_a.IDR = 0xf0U;
    ABoxBoardIo_Poll(&io, 1U);
    ABoxBoardIo_Poll(&io, 21U);
    assert(io.inputs.stable_mask == 0xffU && io.inputs.valid_mask == 0xffU);
    return 0;
}
