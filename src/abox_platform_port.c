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
