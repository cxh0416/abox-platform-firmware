#include "ota_flash.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "abox_platform_port.h"

static uint32_t g_write_addr;
static uint8_t g_flash_started;
static uint8_t g_pending[4];
static uint8_t g_pending_len;

static const ABoxFlashLayout *ota_layout(void)
{
    return ABox_PlatformFlashLayoutGet();
}

static void ota_log(const char *fmt, ...)
{
    char line[192];
    va_list ap;
    int n;

    if (!fmt) return;
    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0) ABox_PortLog("%s", line);
}

static uint8_t OTA_Flash_ProgramWordBytes(const uint8_t bytes[4])
{
    const ABoxFlashLayout *layout = ota_layout();

    if (!layout || (g_write_addr < layout->app_start_addr) || (g_write_addr >= layout->ota_info_addr))
    {
        ota_log("[FLASH] overflow addr=0x%08lX\r\n", (unsigned long)g_write_addr);
        ABox_PortFlashEnd();
        g_flash_started = 0U;
        return 0U;
    }

    if (!ABox_PortFlashWrite(g_write_addr, bytes, 4U))
    {
        ota_log("[FLASH] write fail addr=0x%08lX\r\n", (unsigned long)g_write_addr);
        ABox_PortFlashEnd();
        g_flash_started = 0U;
        return 0U;
    }

    g_write_addr += 4U;
    return 1U;
}

uint8_t OTA_Flash_Begin(void)
{
    const ABoxFlashLayout *layout = ota_layout();
    uint32_t address;

    if (!layout || (layout->ota_info_addr <= layout->app_start_addr) || !ABox_PortFlashBegin()) return 0U;

    if (g_flash_started) ABox_PortFlashEnd();
    g_flash_started = 0U;
    g_write_addr = layout->app_start_addr;
    g_pending_len = 0U;
    memset(g_pending, 0xFF, sizeof(g_pending));

    for (address = layout->app_start_addr; address < layout->ota_info_addr; address += layout->flash_page_size)
    {
        if (!ABox_PortFlashErasePage(address))
        {
            ota_log("[FLASH] erase fail page=0x%08lX\r\n", (unsigned long)address);
            ABox_PortFlashEnd();
            return 0U;
        }
    }

    g_flash_started = 1U;
    ota_log("[FLASH] erase ok, start=0x%08lX\r\n", (unsigned long)layout->app_start_addr);
    return 1U;
}

uint8_t OTA_Flash_Write(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0U)) return 1U;
    if (!g_flash_started) return 0U;

    for (i = 0U; i < len; i++)
    {
        g_pending[g_pending_len++] = data[i];
        if (g_pending_len >= 4U)
        {
            if (!OTA_Flash_ProgramWordBytes(g_pending)) return 0U;
            g_pending_len = 0U;
            memset(g_pending, 0xFF, sizeof(g_pending));
        }
    }
    return 1U;
}

void OTA_Flash_End(void)
{
    if (g_flash_started)
    {
        if (g_pending_len > 0U)
        {
            while (g_pending_len < 4U) g_pending[g_pending_len++] = 0xFFU;
            if (!OTA_Flash_ProgramWordBytes(g_pending))
            {
                g_pending_len = 0U;
                memset(g_pending, 0xFF, sizeof(g_pending));
                return;
            }
        }
        ABox_PortFlashEnd();
        ota_log("[FLASH] write done, end=0x%08lX\r\n", (unsigned long)g_write_addr);
    }

    g_flash_started = 0U;
    g_pending_len = 0U;
    memset(g_pending, 0xFF, sizeof(g_pending));
}

void OTA_Flash_Abort(void)
{
    if (g_flash_started)
    {
        ABox_PortFlashEnd();
        ota_log("[FLASH] abort, end=0x%08lX\r\n", (unsigned long)g_write_addr);
    }
    g_flash_started = 0U;
    g_pending_len = 0U;
    memset(g_pending, 0xFF, sizeof(g_pending));
}

uint32_t OTA_Flash_GetWrittenSize(void)
{
    const ABoxFlashLayout *layout = ota_layout();
    if (!layout || (g_write_addr < layout->app_start_addr)) return 0U;
    return g_write_addr - layout->app_start_addr;
}
