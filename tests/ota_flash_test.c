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
static uint32_t g_begin_count;
static uint32_t g_end_count;
static uint32_t g_fail_erase_at;
static uint32_t g_fail_write_at;
static uint8_t g_unlocked;

static int addr_to_index(uint32_t address, uint32_t length, uint32_t *index)
{
    uint32_t offset;
    if (address < 0x08000000U) return 0;
    offset = address - 0x08000000U;
    if (offset > TEST_FLASH_SIZE || length > TEST_FLASH_SIZE - offset) return 0;
    *index = offset;
    return 1;
}

static uint32_t fake_tick(void *context) { (void)context; return 1U; }
static int fake_uart(void *context, const uint8_t *data, uint32_t length)
{
    (void)context; (void)data; (void)length; return 1;
}
static int fake_begin(void *context)
{
    (void)context;
    g_begin_count++;
    if (g_unlocked) return 0;
    g_unlocked = 1U;
    return 1;
}
static int fake_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    (void)context;
    g_write_count++;
    if (g_fail_write_at != 0U && g_write_count == g_fail_write_at) return 0;
    if (!g_unlocked || !addr_to_index(address, length, &index)) return 0;
    memcpy(&g_flash[index], data, length);
    return 1;
}
static int fake_erase(void *context, uint32_t address)
{
    uint32_t index;
    (void)context;
    g_erase_count++;
    if (g_fail_erase_at != 0U && g_erase_count == g_fail_erase_at) return 0;
    if (!g_unlocked || !addr_to_index(address, TEST_PAGE_SIZE, &index)) return 0;
    memset(&g_flash[index], 0xFF, TEST_PAGE_SIZE);
    return 1;
}
static void fake_end(void *context) { (void)context; g_end_count++; g_unlocked = 0U; }
static void fake_enter(void *context) { (void)context; }
static void fake_exit(void *context) { (void)context; }
static void fake_log(void *context, const char *line) { (void)context; (void)line; }

static const ABoxFlashLayout g_layout = { TEST_APP_ADDR, TEST_OTA_ADDR, TEST_PAGE_SIZE };
static const ABoxPlatformPort g_port = {
    0, fake_tick, fake_uart, fake_begin, fake_write, fake_erase, fake_end,
    fake_enter, fake_exit, fake_log, 0, &g_layout, 0, 0
};

static void reset_fake(void)
{
    memset(g_flash, 0x00, sizeof(g_flash));
    g_erase_count = 0U;
    g_write_count = 0U;
    g_begin_count = 0U;
    g_end_count = 0U;
    g_fail_erase_at = 0U;
    g_fail_write_at = 0U;
    g_unlocked = 0U;
}

static int test_happy_path(void)
{
    const uint8_t payload[5] = { 1U, 2U, 3U, 4U, 5U };

    reset_fake();
    if (!OTA_Flash_Begin()) return 1;
    if (!OTA_Flash_Write(payload, sizeof(payload))) return 2;
    OTA_Flash_End();
    if (g_erase_count != 4U || g_write_count != 2U || g_unlocked) return 3;
    if (memcmp(&g_flash[0x20], payload, sizeof(payload)) != 0) return 4;
    if (g_flash[0x25] != 0xFFU || OTA_Flash_GetWrittenSize() != 8U) return 5;
    return 0;
}

static int test_repeated_begin(void)
{
    reset_fake();
    if (!OTA_Flash_Begin() || !OTA_Flash_Begin()) return 1;
    if (g_begin_count != 2U || g_end_count != 1U || g_erase_count != 8U) return 2;
    OTA_Flash_Abort();
    return g_end_count == 2U && !g_unlocked ? 0 : 3;
}

static int test_failures(void)
{
    uint8_t data[132];
    uint32_t i;

    reset_fake();
    g_fail_erase_at = 2U;
    if (OTA_Flash_Begin() || g_unlocked) return 1;

    reset_fake();
    g_fail_write_at = 1U;
    if (!OTA_Flash_Begin()) return 2;
    if (OTA_Flash_Write((const uint8_t *)"1234", 4U) || g_unlocked) return 3;

    reset_fake();
    for (i = 0U; i < sizeof(data); i++) data[i] = (uint8_t)i;
    if (!OTA_Flash_Begin()) return 4;
    if (OTA_Flash_Write(data, sizeof(data)) || g_unlocked) return 5;

    reset_fake();
    if (!OTA_Flash_Begin()) return 6;
    if (!OTA_Flash_Write(data, 2U)) return 7;
    OTA_Flash_Abort();
    if (g_unlocked || OTA_Flash_Write(data, 1U)) return 8;
    return 0;
}

int main(void)
{
    if (!ABox_PlatformPortBind(&g_port)) return 10;
    if (test_happy_path() != 0) return 11;
    if (test_repeated_begin() != 0) return 12;
    if (test_failures() != 0) return 13;
    return 0;
}
