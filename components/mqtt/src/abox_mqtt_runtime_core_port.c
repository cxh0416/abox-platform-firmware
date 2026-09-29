#include "abox_mqtt_runtime.h"

static const ABoxMqttRuntimeCoreApi *core(void *user)
{
    return (const ABoxMqttRuntimeCoreApi *)user;
}

static void set_paused(void *user, uint8_t value)
{ core(user)->set_paused(value); }
static void set_security_ready(void *user, uint8_t value)
{ core(user)->set_security_ready(value); }
static int apply_config(void *user, const ABoxMqttConfig *config)
{ return core(user)->apply_config(config); }
static void task(void *user)
{ core(user)->task(); }
static void modem_reset(void *user)
{ core(user)->modem_reset(); }
static int stop(void *user, uint32_t now)
{ return core(user)->stop(now); }
static uint8_t ready(void *user)
{ return core(user)->ready(); }
static uint8_t connected(void *user)
{ return core(user)->connected(); }

int ABoxMqttRuntimePort_BindCore(ABoxMqttRuntimePort *port,
                                  const ABoxMqttRuntimeCoreApi *api)
{
    if (!port || !api || !api->set_paused || !api->set_security_ready ||
        !api->apply_config || !api->task || !api->modem_reset ||
        !api->stop || !api->ready || !api->connected) return 0;
    port->user = (void *)api;
    port->set_paused = set_paused;
    port->set_security_ready = set_security_ready;
    port->apply_config = apply_config;
    port->mqtt_task = task;
    port->mqtt_modem_reset = modem_reset;
    port->mqtt_stop = stop;
    port->mqtt_ready = ready;
    port->mqtt_connected = connected;
    return 1;
}
