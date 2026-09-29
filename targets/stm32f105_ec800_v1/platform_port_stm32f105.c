#include <string.h>

#include "abox_platform_port.h"
#include "abox_stm32f105_ec800_port.h"
#include "boot_cfg.h"
#include "ec_power.h"
#include "gpio.h"
#include "main.h"
#include "usart.h"

static const ABoxFlashLayout g_flash_layout = {
    APP_START_ADDR, OTA_INFO_ADDR, OTA_FLASH_PAGE_SIZE
};

static uint32_t get_tick(void *context) { (void)context; return HAL_GetTick(); }

static int uart_write(void *context, const uint8_t *data, uint32_t length)
{
    (void)context;
    if (!data || length == 0U || length > 0xFFFFU) return 0;
    return HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)length, 1000U) == HAL_OK;
}

static int flash_begin(void *context)
{
    (void)context;
    return HAL_FLASH_Unlock() == HAL_OK;
}

static int flash_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t word;
    (void)context;
    if (!data || length != sizeof(word)) return 0;
    memcpy(&word, data, sizeof(word));
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) == HAL_OK;
}

static int flash_erase_page(void *context, uint32_t address)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    (void)context;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1U;
    return HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK;
}

static void flash_end(void *context) { (void)context; (void)HAL_FLASH_Lock(); }
static void enter_critical(void *context) { (void)context; __disable_irq(); }
static void exit_critical(void *context) { (void)context; __enable_irq(); }
static void log_write(void *context, const char *line) { (void)context; (void)line; }
static int ota_is_reading_raw(void *context) { (void)context; return 0; }

static void ec_power_write(void *context, uint8_t level)
{
    (void)context;
    HAL_GPIO_WritePin(EN_3_8V_GPIO_Port, EN_3_8V_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void ec_pwrkey_write(void *context, uint8_t level)
{
    (void)context;
    HAL_GPIO_WritePin(EC_PWR_GPIO_Port, EC_PWR_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void ABoxStm32F105Ec800Port_Init(void)
{
    static const ABoxPlatformPort port = {
        0, get_tick, uart_write, flash_begin, flash_write,
        flash_erase_page, flash_end, enter_critical, exit_critical,
        log_write, ota_is_reading_raw, &g_flash_layout,
        ec_power_write, ec_pwrkey_write
    };
    static const ABoxEcPowerConfig power = {6000U, 2000U, 3000U, 0U, 1U};

    (void)ABox_PlatformPortBind(&port);
    (void)ABox_EcPower_SetConfig(&power);
}
