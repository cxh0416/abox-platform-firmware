#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "abox_mqtt_runtime.h"

typedef struct {
    uint32_t now;
    uint8_t paused, security, connected, ready;
    unsigned writes, revokes;
    char last_command[128];
} Fixture;

static uint32_t tick(void *context) { return ((Fixture *)context)->now; }
static int write_at(void *context, const uint8_t *data, uint16_t length,
                    uint32_t timeout)
{
    Fixture *f = context;
    (void)timeout;
    assert(length < sizeof(f->last_command));
    memcpy(f->last_command, data, length);
    f->last_command[length] = '\0';
    ++f->writes;
    return 1;
}
static void set_paused(void *context, uint8_t value) { ((Fixture *)context)->paused = value; }
static void set_security(void *context, uint8_t value) { ((Fixture *)context)->security = value; }
static int apply(void *context, const ABoxMqttConfig *config)
{ (void)context; return config && config->host && config->port; }
static void mqtt_task(void *context) { (void)context; }
static uint8_t connected(void *context) { return ((Fixture *)context)->connected; }
static uint8_t ready(void *context) { return ((Fixture *)context)->ready; }
static uint8_t yes(void *context) { (void)context; return 1U; }
static void revoke(void *context) { ++((Fixture *)context)->revokes; }

int main(void)
{
    Fixture f = {0};
    ABoxEc800At at;
    ABoxEc800AtPort at_port = {0};
    ABoxMqttRuntime runtime;
    ABoxMqttRuntimePort port = {0};
    ABoxMqttRuntimeOptions options = {0};
    const ABoxMqttConfig first = {"first.example", 1883U, "u", "p", 0U, 0U};
    const ABoxMqttConfig next = {"next.example", 1884U, "u", "p", 0U, 0U};
    const uint8_t disc[] = "OK\r\n+QMTDISC: 0,0\r\n";
    const uint8_t close[] = "OK\r\n+QMTCLOSE: 0,0\r\n";
    const uint8_t disc_tls[] = "OK\r\n+QMTDISC: 1,0\r\n";
    const uint8_t close_tls[] = "OK\r\n+QMTCLOSE: 1,0\r\n";

    at_port.context = &f; at_port.tick_ms = tick; at_port.write = write_at;
    assert(ABoxEc800At_Init(&at, &at_port));
    port.user = &f; port.set_paused = set_paused; port.set_security_ready = set_security;
    port.apply_config = apply; port.mqtt_task = mqtt_task;
    port.mqtt_ready = ready; port.mqtt_connected = connected;
    port.ca_ready = yes; port.time_valid = yes; port.before_network_change = revoke;
    options.owner = 20U; options.command_timeout_ms = 5000U;
    options.tls_timeout_ms = 30000U; options.connect_timeout_ms = 60000U;
    options.subscribe_timeout_ms = 60000U; options.ca_file = "UFS:ca.pem";
    options.ca_revision = 1U;
    options.plain_client = 0U; options.tls_client = 1U;
    options.tls_context = 2U; options.supported_tls_context_mask = 4U;
    assert(ABoxMqttRuntime_Init(&runtime, &at, &port, &options, &first, 0U));
    assert(runtime.state == ABOX_MQTT_RUNTIME_PREPARED && !f.paused && f.security);
    ABoxMqttRuntime_Poll(&runtime, 1U);
    assert(runtime.state == ABOX_MQTT_RUNTIME_CONNECT);
    f.connected = 1U; ABoxMqttRuntime_Poll(&runtime, 2U);
    f.ready = 1U; ABoxMqttRuntime_Poll(&runtime, 3U);
    assert(ABoxMqttRuntime_IsReady(&runtime));
    assert(strcmp(ABoxMqttRuntime_ActiveConfig(&runtime)->host, "first.example") == 0);

    assert(ABoxMqttRuntime_Stage(&runtime, &next));
    assert(ABoxMqttRuntime_Activate(&runtime, 4U));
    assert(f.paused && !f.security && f.revokes == 1U);
    ABoxEc800At_Task(&at);
    assert(strstr(f.last_command, "AT+QMTDISC=0") != 0);
    ABoxEc800At_Feed(&at, disc, (uint16_t)(sizeof(disc) - 1U));
    ABoxMqttRuntime_Poll(&runtime, 5U);
    ABoxMqttRuntime_Poll(&runtime, 6U);
    ABoxEc800At_Task(&at);
    assert(strstr(f.last_command, "AT+QMTCLOSE=0") != 0);
    ABoxEc800At_Feed(&at, close, (uint16_t)(sizeof(close) - 1U));
    ABoxMqttRuntime_Poll(&runtime, 7U);
    assert(runtime.state == ABOX_MQTT_RUNTIME_PREPARED);
    f.connected = f.ready = 0U;
    ABoxMqttRuntime_Poll(&runtime, 8U);
    f.connected = 1U; ABoxMqttRuntime_Poll(&runtime, 9U);
    f.ready = 1U; ABoxMqttRuntime_Poll(&runtime, 10U);
    assert(ABoxMqttRuntime_IsReady(&runtime));
    assert(strcmp(ABoxMqttRuntime_ActiveConfig(&runtime)->host, "next.example") == 0);
    assert(ABoxEc800At_Submit(&at, "AT+CSQ", 30U, ABOX_EC800_PRIORITY_NORMAL,
                             5000U, 0, 0));
    ABoxEc800At_Task(&at);
    assert(ABoxMqttRuntime_StopFirst(&runtime, 11U) == 0);
    assert(f.paused && !f.security && f.revokes == 2U);
    ABoxMqttRuntime_Poll(&runtime, 12U);
    assert(ABoxMqttRuntime_GetState(&runtime) == ABOX_MQTT_RUNTIME_STOP_WAIT_DRAIN);
    ABoxEc800At_Feed(&at, (const uint8_t *)"OK\r\n", 4U);
    ABoxMqttRuntime_Poll(&runtime, 13U);
    assert(ABoxMqttRuntime_GetState(&runtime) == ABOX_MQTT_RUNTIME_STOP_DISCONNECT);
    ABoxMqttRuntime_Poll(&runtime, 14U);
    ABoxEc800At_Task(&at);
    assert(strstr(f.last_command, "AT+QMTDISC=0") != 0);
    ABoxEc800At_Feed(&at, disc, (uint16_t)(sizeof(disc) - 1U));
    ABoxMqttRuntime_Poll(&runtime, 15U);
    ABoxMqttRuntime_Poll(&runtime, 16U);
    ABoxEc800At_Task(&at);
    assert(strstr(f.last_command, "AT+QMTCLOSE=0") != 0);
    ABoxEc800At_Feed(&at, close, (uint16_t)(sizeof(close) - 1U));
    ABoxMqttRuntime_Poll(&runtime, 17U);
    assert(ABoxMqttRuntime_StopFirst(&runtime, 18U) == 1);
    assert(ABoxMqttRuntime_GetState(&runtime) == ABOX_MQTT_RUNTIME_UNENROLLED);
    assert(ABoxMqttRuntime_ActiveConfig(&runtime) == 0);
    ABoxMqttRuntime_Block(&runtime);
    assert(!ABoxMqttRuntime_Stage(&runtime, &first));
    assert(ABoxMqttRuntime_RecoveryComplete(&runtime));
    assert(ABoxMqttRuntime_Stage(&runtime, &first));

    {
        Fixture tls_fixture = {0};
        ABoxEc800At tls_at;
        ABoxMqttRuntime tls_runtime;
        const ABoxMqttConfig tls_config = {"secure.example", 8883U, "u", "p", 1U, 0U};
        uint32_t now;
        at_port.context = &tls_fixture;
        port.user = &tls_fixture;
        assert(ABoxEc800At_Init(&tls_at, &at_port));
        assert(ABoxMqttRuntime_Init(&tls_runtime, &tls_at, &port, &options,
                                    &tls_config, 0U));
        for (now = 1U; now < 80U && !ABoxMqttRuntime_IsReady(&tls_runtime); ++now) {
            tls_fixture.now = now;
            if (tls_runtime.state == ABOX_MQTT_RUNTIME_CONNECT ||
                tls_runtime.state == ABOX_MQTT_RUNTIME_CONNECTED) {
                tls_fixture.connected = tls_fixture.ready = 1U;
            }
            ABoxMqttRuntime_Poll(&tls_runtime, now);
            ABoxEc800At_Task(&tls_at);
            if (tls_at.active_valid) {
                const uint8_t ok[] = "OK\r\n";
                ABoxEc800At_Feed(&tls_at, ok, (uint16_t)(sizeof(ok) - 1U));
            }
        }
        assert(ABoxMqttRuntime_IsReady(&tls_runtime));
        assert(ABoxMqttRuntime_IsTlsActive(&tls_runtime));
        assert(tls_runtime.lease.generation != 0U);
        assert(ABoxMqttRuntime_StopFirst(&tls_runtime, now++) == 0);
        for (; now < 120U && ABoxMqttRuntime_StopFirst(&tls_runtime, now) == 0; ++now) {
            tls_fixture.now = now;
            ABoxMqttRuntime_Poll(&tls_runtime, now);
            ABoxEc800At_Task(&tls_at);
            if (tls_at.active_valid) {
                const uint8_t ok[] = "OK\r\n";
                if (strstr(tls_fixture.last_command, "AT+QMTDISC=1"))
                    ABoxEc800At_Feed(&tls_at, disc_tls, (uint16_t)(sizeof(disc_tls) - 1U));
                else if (strstr(tls_fixture.last_command, "AT+QMTCLOSE=1"))
                    ABoxEc800At_Feed(&tls_at, close_tls, (uint16_t)(sizeof(close_tls) - 1U));
                else
                    ABoxEc800At_Feed(&tls_at, ok, (uint16_t)(sizeof(ok) - 1U));
            }
        }
        assert(ABoxMqttRuntime_StopFirst(&tls_runtime, now) == 1);
        assert(!ABoxMqttRuntime_IsTlsActive(&tls_runtime));
        assert(tls_runtime.lease.generation == 0U);
    }
    {
        Fixture reset_fixture = {0};
        ABoxEc800At reset_at;
        ABoxMqttRuntime reset_runtime;
        at_port.context = &reset_fixture;
        port.user = &reset_fixture;
        assert(ABoxEc800At_Init(&reset_at, &at_port));
        assert(ABoxMqttRuntime_Init(&reset_runtime, &reset_at, &port, &options,
                                    &first, 0U));
        reset_fixture.connected = reset_fixture.ready = 1U;
        ABoxMqttRuntime_Poll(&reset_runtime, 1U);
        ABoxMqttRuntime_Poll(&reset_runtime, 2U);
        ABoxMqttRuntime_Poll(&reset_runtime, 3U);
        assert(ABoxMqttRuntime_IsReady(&reset_runtime));
        assert(ABoxMqttRuntime_StopFirst(&reset_runtime, 4U) == 0);
        ABoxMqttRuntime_OnModemReset(&reset_runtime, 5U);
        assert(ABoxMqttRuntime_GetState(&reset_runtime) == ABOX_MQTT_RUNTIME_BLOCKED);
        assert(!ABoxMqttRuntime_Stage(&reset_runtime, &next));
    }
    return 0;
}
