#include "ec_power.h"

#include "abox_platform_port.h"

typedef enum
{
    EC_PWR_IDLE = 0,
    EC_PWR_RESTART_WAIT,
    EC_PWR_OFF_HOLD,
    EC_PWR_RAIL_STABLE,
    EC_PWR_KEY_HOLD,
    EC_PWR_DONE
} EcPowerState;

static const ABoxEcPowerConfig g_default_config = {
    1000U, 1000U, 2000U, 0U, 0U
};
static ABoxEcPowerConfig g_config;
static EcPowerState g_state = EC_PWR_IDLE;
static uint32_t g_state_tick;

static void ec_power_outputs_off(void)
{
    ABox_PortEcPwrkeyWrite(0U);
    ABox_PortEcPowerEnableWrite(0U);
}

static void ec_power_start_off_hold(uint32_t now)
{
    g_state_tick = now;
    g_state = EC_PWR_OFF_HOLD;
}

uint8_t ABox_EcPower_SetConfig(const ABoxEcPowerConfig *config)
{
    if (!config || config->off_hold_ms == 0U || config->rail_stable_ms == 0U ||
        config->key_hold_ms == 0U)
        return 0U;

    g_config = *config;
    return 1U;
}

void EC_Power_Init(void)
{
    if (g_config.off_hold_ms == 0U) g_config = g_default_config;
    g_state = EC_PWR_IDLE;
    g_state_tick = ABox_PortGetTickMs();
    if (g_config.init_force_off) ec_power_outputs_off();
}

void EC_Power_ForceOff(void)
{
    ec_power_outputs_off();
    g_state = EC_PWR_IDLE;
    g_state_tick = ABox_PortGetTickMs();
}

void EC_Power_StartOnSequence(void)
{
    uint32_t now = ABox_PortGetTickMs();
    ec_power_outputs_off();
    ec_power_start_off_hold(now);
}

void EC_Power_StartRestartSequence(void)
{
    uint32_t now = ABox_PortGetTickMs();
    ec_power_outputs_off();
    g_state_tick = now;
    g_state = (g_config.restart_off_ms > 0U) ? EC_PWR_RESTART_WAIT : EC_PWR_OFF_HOLD;
}

void EC_Power_Task(void)
{
    uint32_t now = ABox_PortGetTickMs();

    switch (g_state)
    {
        case EC_PWR_IDLE:
        case EC_PWR_DONE:
            break;

        case EC_PWR_RESTART_WAIT:
            if ((now - g_state_tick) >= g_config.restart_off_ms)
                ec_power_start_off_hold(now);
            break;

        case EC_PWR_OFF_HOLD:
            if ((now - g_state_tick) >= g_config.off_hold_ms)
            {
                ABox_PortEcPowerEnableWrite(1U);
                g_state_tick = now;
                g_state = EC_PWR_RAIL_STABLE;
            }
            break;

        case EC_PWR_RAIL_STABLE:
            if ((now - g_state_tick) >= g_config.rail_stable_ms)
            {
                ABox_PortEcPwrkeyWrite(1U);
                g_state_tick = now;
                g_state = EC_PWR_KEY_HOLD;
            }
            break;

        case EC_PWR_KEY_HOLD:
            if ((now - g_state_tick) >= g_config.key_hold_ms)
            {
                ABox_PortEcPwrkeyWrite(0U);
                g_state = EC_PWR_DONE;
            }
            break;

        default:
            g_state = EC_PWR_IDLE;
            break;
    }
}

uint8_t EC_Power_IsBusy(void)
{
    return (g_state != EC_PWR_IDLE && g_state != EC_PWR_DONE) ? 1U : 0U;
}

uint8_t EC_Power_IsOnDone(void) { return (g_state == EC_PWR_DONE) ? 1U : 0U; }
uint8_t EC_Power_IsDone(void) { return EC_Power_IsOnDone(); }
