#include <stdint.h>
#include <string.h>

#include "abox_platform_port.h"
#include "ota_ec_at.h"

static uint32_t g_tick;
static uint8_t g_raw_mode;
static char g_tx[256];
static uint32_t g_tx_len;
static uint32_t g_line_count;
static char g_last_line[64];
static uint8_t g_raw[256];
static uint32_t g_raw_len;

static uint32_t fake_tick(void *context) { (void)context; return g_tick; }
static int fake_uart(void *context, const uint8_t *data, uint32_t length)
{
    (void)context;
    if (length > sizeof(g_tx) - g_tx_len) return 0;
    memcpy(&g_tx[g_tx_len], data, length);
    g_tx_len += length;
    return 1;
}
static int fake_flash_begin(void *context) { (void)context; return 1; }
static int fake_flash_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{ (void)context; (void)address; (void)data; (void)length; return 1; }
static int fake_flash_erase(void *context, uint32_t address)
{ (void)context; (void)address; return 1; }
static void fake_flash_end(void *context) { (void)context; }
static void fake_critical(void *context) { (void)context; }
static void fake_log(void *context, const char *line) { (void)context; (void)line; }
static int fake_raw_mode(void *context) { (void)context; return g_raw_mode ? 1 : 0; }

static const ABoxFlashLayout g_layout = { 0x08008000U, 0x0803F800U, 0x800U };
static const ABoxPlatformPort g_port = {
    0, fake_tick, fake_uart, fake_flash_begin, fake_flash_write, fake_flash_erase,
    fake_flash_end, fake_critical, fake_critical, fake_log, fake_raw_mode,
    &g_layout, 0, 0
};

static void line_hook(const char *line)
{
    size_t length = strlen(line);
    if (length >= sizeof(g_last_line)) length = sizeof(g_last_line) - 1U;
    memcpy(g_last_line, line, length);
    g_last_line[length] = '\0';
    g_line_count++;
    if (strcmp(line, "CONNECT") == 0) g_raw_mode = 1U;
}

static void raw_hook(const uint8_t *data, uint16_t length)
{
    if (length > sizeof(g_raw) - g_raw_len) length = (uint16_t)(sizeof(g_raw) - g_raw_len);
    memcpy(&g_raw[g_raw_len], data, length);
    g_raw_len += length;
}

static void reset_state(void)
{
    g_tick = 0U;
    g_raw_mode = 0U;
    g_tx_len = 0U;
    g_line_count = 0U;
    g_last_line[0] = '\0';
    g_raw_len = 0U;
    memset(g_tx, 0, sizeof(g_tx));
    memset(g_raw, 0, sizeof(g_raw));
    OTA_EC_AT_Init();
    OTA_EC_RegisterLineHook(line_hook);
    OTA_EC_RegisterRawHook(raw_hook);
}

static int test_lines_and_queries(void)
{
    uint8_t first[] = "AT\r";
    uint8_t second[] = "\nOK\r\n";
    uint8_t status[] = "+CPIN: READY\r\n+CSQ: 18,99\r\n+CEREG: 2,5\r\n";

    if (!OTA_EC_TestAT()) return 1;
    if (g_tx_len != 4U || memcmp(g_tx, "AT\r\n", 4U) != 0) return 2;
    OTA_EC_AT_OnRx(first, (uint16_t)(sizeof(first) - 1U));
    OTA_EC_AT_OnRx(second, (uint16_t)(sizeof(second) - 1U));
    OTA_EC_AT_Task();
    if (!OTA_EC_IsReady()) return 3;

    OTA_EC_RequestCpin();
    OTA_EC_RequestCsq();
    OTA_EC_RequestCereg();
    OTA_EC_AT_OnRx(status, (uint16_t)(sizeof(status) - 1U));
    if (!OTA_EC_IsCpinDone() || !OTA_EC_IsSimReady()) return 4;
    if (!OTA_EC_IsCsqDone() || OTA_EC_GetCsq() != 18) return 5;
    if (!OTA_EC_IsCeregDone() || !OTA_EC_IsNetReady() || OTA_EC_GetCeregStat() != 5) return 6;

    OTA_EC_AT_OnRx((uint8_t *)"+CME ERROR: 515\r\n", 17U);
    if (!OTA_EC_HasCmeError() || OTA_EC_GetCmeErrorCode() != 515) return 7;
    return 0;
}

static int test_raw_transition(void)
{
    uint8_t first[] = "CONNECT\r\n";
    uint8_t second[] = "A\r\nB\0\r\n";
    static const uint8_t expected[] = { 'A', '\r', '\n', 'B', '\0', '\r', '\n' };

    OTA_EC_AT_OnRx(first, (uint16_t)(sizeof(first) - 1U));
    if (g_raw_len != 0U) return 1;
    OTA_EC_AT_OnRx(second, (uint16_t)(sizeof(second) - 1U));
    if (g_raw_len != sizeof(expected) || memcmp(g_raw, expected, sizeof(expected)) != 0) return 2;
    return 0;
}

static int test_timeout(void)
{
    reset_state();
    if (!OTA_EC_TestAT()) return 1;
    g_tick = 2999U;
    OTA_EC_AT_Task();
    if (OTA_EC_IsReady()) return 2;
    g_tick = 3000U;
    OTA_EC_AT_Task();
    return OTA_EC_IsReady() ? 3 : 0;
}

int main(void)
{
    if (!ABox_PlatformPortBind(&g_port)) return 10;
    reset_state();
    if (test_lines_and_queries() != 0) return 11;
    if (test_raw_transition() != 0) return 12;
    if (test_timeout() != 0) return 13;
    return 0;
}
