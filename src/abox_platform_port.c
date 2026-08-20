#include "abox_platform_port.h"

#include <stdarg.h>
#include <stdio.h>

static ABoxPlatformPort g_port;
static uint8_t g_port_bound;

int ABox_PlatformPortBind(const ABoxPlatformPort *port)
{
    if (!ABox_PlatformPortIsValid(port)) return 0;
    g_port = *port;
    g_port_bound = 1U;
    return 1;
}

const ABoxPlatformPort *ABox_PlatformPortGet(void)
{
    return g_port_bound ? &g_port : 0;
}

const ABoxFlashLayout *ABox_PlatformFlashLayoutGet(void)
{
    return g_port_bound ? g_port.flash_layout : 0;
}

uint32_t ABox_PortGetTickMs(void)
{
    return (g_port_bound && g_port.get_tick_ms) ? g_port.get_tick_ms(g_port.context) : 0U;
}

int ABox_PortUartWrite(const uint8_t *data, uint32_t length)
{
    return (g_port_bound && g_port.uart_write) ? g_port.uart_write(g_port.context, data, length) : 0;
}

int ABox_PortFlashBegin(void)
{
    return (g_port_bound && g_port.flash_begin) ? g_port.flash_begin(g_port.context) : 0;
}

int ABox_PortFlashWrite(uint32_t address, const uint8_t *data, uint32_t length)
{
    return (g_port_bound && g_port.flash_write) ? g_port.flash_write(g_port.context, address, data, length) : 0;
}

int ABox_PortFlashErasePage(uint32_t address)
{
    return (g_port_bound && g_port.flash_erase_page) ? g_port.flash_erase_page(g_port.context, address) : 0;
}

void ABox_PortFlashEnd(void)
{
    if (g_port_bound && g_port.flash_end) g_port.flash_end(g_port.context);
}

int ABox_PortFlashRead(uint32_t address, uint8_t *data, uint32_t length)
{
    if (!data || length == 0U) return 0;
    if (g_port_bound && g_port.flash_read) return g_port.flash_read(g_port.context, address, data, length);
#if defined(__arm__) || defined(__thumb__)
    {
        const uint8_t *source = (const uint8_t *)(uintptr_t)address;
        uint32_t i;
        for (i = 0U; i < length; ++i) data[i] = source[i];
        return 1;
    }
#else
    return 0;
#endif
}

void ABox_PortLog(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    int n;

    if (!g_port_bound || !g_port.log_write || !fmt) return;

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    line[sizeof(line) - 1U] = '\0';
    g_port.log_write(g_port.context, line);
}

int ABox_PortOtaIsReadingRaw(void)
{
    return (g_port_bound && g_port.ota_is_reading_raw) ? g_port.ota_is_reading_raw(g_port.context) : 0;
}

void ABox_PortEcPowerEnableWrite(uint8_t level)
{
    if (g_port_bound && g_port.ec_power_enable_write)
        g_port.ec_power_enable_write(g_port.context, level ? 1U : 0U);
}

void ABox_PortEcPwrkeyWrite(uint8_t level)
{
    if (g_port_bound && g_port.ec_pwrkey_write)
        g_port.ec_pwrkey_write(g_port.context, level ? 1U : 0U);
}
