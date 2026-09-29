#include <assert.h>
#include <string.h>

#include "abox_mqtt_runtime.h"

static unsigned paused, security, tasks, resets, stops;
static const ABoxMqttConfig *applied;
static void pause_transport(uint8_t value) { paused = value; }
static void security_ready(uint8_t value) { security = value; }
static uint8_t apply(const ABoxMqttConfig *config)
{ applied = config; return config && config->port == 1883U; }
static void task(void) { ++tasks; }
static void reset(void) { ++resets; }
static int stop(uint32_t now) { stops = now; return now == 42U; }
static uint8_t ready(void) { return 1U; }
static uint8_t connected(void) { return 0U; }
static void before_change(void *user) { assert(user != 0); }

int main(void)
{
    ABoxMqttRuntimePort port = {0};
    ABoxMqttRuntimeCoreApi api = {
        pause_transport, security_ready, apply, task, reset,
        stop, ready, connected
    };
    ABoxMqttConfig config = {"broker", 1883U, "user", "pass", 0U, 0U};
    port.before_network_change = before_change;
    assert(!ABoxMqttRuntimePort_BindCore(0, &api));
    assert(!ABoxMqttRuntimePort_BindCore(&port, 0));
    api.stop = 0;
    assert(!ABoxMqttRuntimePort_BindCore(&port, &api));
    assert(!port.user && !port.set_paused);
    api.stop = stop;
    assert(ABoxMqttRuntimePort_BindCore(&port, &api));
    assert(port.user == &api && port.before_network_change == before_change);
    port.set_paused(port.user, 1U);
    port.set_security_ready(port.user, 1U);
    assert(port.apply_config(port.user, &config));
    assert(applied == &config && paused == 1U && security == 1U);
    port.mqtt_task(port.user);
    port.mqtt_modem_reset(port.user);
    assert(port.mqtt_stop(port.user, 42U) == 1);
    assert(tasks == 1U && resets == 1U && stops == 42U);
    assert(port.mqtt_ready(port.user) && !port.mqtt_connected(port.user));
    port.before_network_change(port.user);
    return 0;
}
