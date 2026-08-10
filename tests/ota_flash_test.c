#include <stdint.h>
#include <string.h>

#include "abox_platform_port.h"
#include "ota_flash.h"

#define TEST_APP_ADDR 0x08000020U
#define TEST_OTA_ADDR 0x080000A0U
#define TEST_PAGE_SIZE 0x20U
#define TEST_FLASH_SIZE 0x100U

static uint8_t g_flash[TEST_FLASH_SIZE];
static uint32_t g_erase_count;
static uint32_t g_write_count;
static uint8_t g_unlocked;

static int addr_to_index(uint32_t address, uint32_t length, uint32_t *index)
{
    if ((address < 0x08000000U) || ((address - 0x08000000U) > TEST_FLASH_SIZE) ||
        (length > TEST_FLASH_SIZE - (address - 0x08000000U))) return 0;
    *index = address - 0x08000000U;
    return 1;
}

static uint32_t fake_tick(void *context) { (void)context; return 1U; }
static int fake_uart(void *context, const uint8_t *data, uint32_t length) { (void)context; (void)data; (void)length; return 1; }
static int fake_begin(void *context) { (void)context; g_unlocked = 1U; return 1; }
static int fake_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    (void)context;
    if (!g_unlocked || !addr_to_index(address, length, &index)) return 0;
    memcpy(&g_flash[index], data, length);
    g_write_count++;
    return 1;
}
static int fake_erase(void *context, uint32_t address)
{
    uint32_t index;
    (void)context;
    if (!g_unlocked || !addr_to_index(address, TEST_PAGE_SIZE, &index)) return 0;
    memset(&g_flash[index], 0xFF, TEST_PAGE_SIZE);
    g_erase_count++;
    return 1;
}
static void fake_end(void *context) { (void)context; g_unlocked = 0U; }
static void fake_enter(void *context) { (void)context; }
static void fake_exit(void *context) { (void)context; }

int main(void)
{
    static const ABoxFlashLayout layout = { TEST_APP_ADDR, TEST_OTA_ADDR, TEST_PAGE_SIZE };
    static const ABoxPlatformPort port = {
        0, fake_tick, fake_uart, fake_begin, fake_write, fake_erase, fake_end,
        fake_enter, fake_exit, 0, 0, &layout
    };
    const uint8_t payload[5] = { 1U, 2U, 3U, 4U, 5U };

    memset(g_flash, 0x00, sizeof(g_flash));
    if (!ABox_PlatformPortBind(&port)) return 1;
    if (!OTA_Flash_Begin()) return 2;
    if (!OTA_Flash_Write(payload, sizeof(payload))) return 3;
    OTA_Flash_End();
    if (g_erase_count != 4U || g_write_count != 2U) return 4;
    if (memcmp(&g_flash[0x20], payload, sizeof(payload)) != 0) return 5;
    if (g_flash[0x25] != 0xFFU || OTA_Flash_GetWrittenSize() != 8U) return 6;
    if (g_flash[0xA0] != 0U) return 7;
    return 0;
}
