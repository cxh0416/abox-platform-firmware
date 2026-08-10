#include <stdint.h>

#include "abox_platform_port.h"
#include "ec_power.h"

static uint32_t g_tick;
static uint8_t g_rail;
static uint8_t g_key;
static uint32_t g_rail_changes;
static uint32_t g_key_changes;

static uint32_t fake_tick(void *context) { (void)context; return g_tick; }
static int fake_uart(void *context, const uint8_t *data, uint32_t length)
{ (void)context; (void)data; (void)length; return 1; }
static int fake_flash_begin(void *context) { (void)context; return 1; }
static int fake_flash_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{ (void)context; (void)address; (void)data; (void)length; return 1; }
static int fake_flash_erase(void *context, uint32_t address)
{ (void)context; (void)address; return 1; }
static void fake_flash_end(void *context) { (void)context; }
static void fake_critical(void *context) { (void)context; }
static void fake_log(void *context, const char *line) { (void)context; (void)line; }
static void fake_rail(void *context, uint8_t level)
{ (void)context; g_rail = level; g_rail_changes++; }
static void fake_key(void *context, uint8_t level)
{ (void)context; g_key = level; g_key_changes++; }

static const ABoxFlashLayout g_layout = { 0x08008000U, 0x0803F800U, 0x800U };
static const ABoxPlatformPort g_port = {
    0, fake_tick, fake_uart, fake_flash_begin, fake_flash_write, fake_flash_erase,
    fake_flash_end, fake_critical, fake_critical, fake_log, 0, &g_layout,
    fake_rail, fake_key
};

static void reset_io(uint32_t tick)
{
    g_tick = tick;
    g_rail = 1U;
    g_key = 1U;
    g_rail_changes = 0U;
    g_key_changes = 0U;
}

static int test_meal_profile(void)
{
    static const ABoxEcPowerConfig config = { 6000U, 2000U, 3000U, 0U, 1U };

    reset_io(0U);
    if (!ABox_EcPower_SetConfig(&config)) return 1;
    EC_Power_Init();
    if (g_rail != 0U || g_key != 0U) return 2;
    EC_Power_StartOnSequence();
    g_tick = 5999U; EC_Power_Task();
    if (g_rail != 0U) return 3;
    g_tick = 6000U; EC_Power_Task();
    if (g_rail != 1U) return 4;
    g_tick = 7999U; EC_Power_Task();
    if (g_key != 0U) return 5;
    g_tick = 8000U; EC_Power_Task();
    if (g_key != 1U || !EC_Power_IsBusy()) return 6;
    g_tick = 10999U; EC_Power_Task();
    if (!EC_Power_IsBusy()) return 7;
    g_tick = 11000U; EC_Power_Task();
    if (g_key != 0U || !EC_Power_IsDone() || EC_Power_IsBusy()) return 8;
    return 0;
}

static int test_boot_restart_profile(void)
{
    static const ABoxEcPowerConfig config = { 1000U, 1000U, 2000U, 3000U, 0U };

    reset_io(0U);
    if (!ABox_EcPower_SetConfig(&config)) return 1;
    EC_Power_Init();
    if (g_rail != 1U || g_key != 1U) return 2;
    EC_Power_StartRestartSequence();
    if (g_rail != 0U || g_key != 0U) return 3;
    g_tick = 2999U; EC_Power_Task();
    if (g_rail != 0U) return 4;
    g_tick = 3000U; EC_Power_Task();
    g_tick = 3999U; EC_Power_Task();
    if (g_rail != 0U) return 5;
    g_tick = 4000U; EC_Power_Task();
    if (g_rail != 1U) return 6;
    g_tick = 5000U; EC_Power_Task();
    if (g_key != 1U) return 7;
    g_tick = 7000U; EC_Power_Task();
    if (g_key != 0U || !EC_Power_IsDone()) return 8;
    EC_Power_ForceOff();
    if (g_rail != 0U || g_key != 0U || EC_Power_IsBusy()) return 9;
    return 0;
}

static int test_tick_wrap(void)
{
    static const ABoxEcPowerConfig config = { 10U, 10U, 10U, 0U, 1U };

    reset_io(0xFFFFFFF8U);
    if (!ABox_EcPower_SetConfig(&config)) return 1;
    EC_Power_Init();
    EC_Power_StartOnSequence();
    g_tick = 2U; EC_Power_Task();
    if (g_rail != 1U) return 2;
    g_tick = 12U; EC_Power_Task();
    if (g_key != 1U) return 3;
    g_tick = 22U; EC_Power_Task();
    return (g_key == 0U && EC_Power_IsDone()) ? 0 : 4;
}

int main(void)
{
    if (!ABox_PlatformPortBind(&g_port)) return 10;
    if (test_meal_profile() != 0) return 11;
    if (test_boot_restart_profile() != 0) return 12;
    if (test_tick_wrap() != 0) return 13;
    return 0;
}
