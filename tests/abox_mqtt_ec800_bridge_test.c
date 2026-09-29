#include "abox_mqtt_ec800_bridge.h"

#include <assert.h>
#include <string.h>

typedef struct {
    uint32_t now;
    unsigned messages, network_changes, confirms;
    char topic[32], payload[32];
} Fixture;

static uint32_t tick(void *context) { return ((Fixture *)context)->now; }
static int write_bytes(void *context, const uint8_t *data,
                       uint16_t length, uint32_t timeout)
{
    (void)context; (void)data; (void)length; (void)timeout;
    return 1;
}
static void message(void *context, const char *topic, const char *payload)
{
    Fixture *fixture = (Fixture *)context;
    ++fixture->messages;
    strcpy(fixture->topic, topic);
    strcpy(fixture->payload, payload);
}
static void network_changed(void *context)
{
    ++((Fixture *)context)->network_changes;
}
static void publish_event(void *context, uint64_t operation,
                          ABoxMqttEc800PublishEvent event)
{
    Fixture *fixture = (Fixture *)context;
    (void)operation;
    if (event == ABOX_MQTT_EC800_PUBLISH_CONFIRMED) ++fixture->confirms;
}

int main(void)
{
    Fixture fixture = {0};
    ABoxEc800At at;
    ABoxEc800AtPort at_port = {0};
    ABoxMqttEc800Bridge bridge;
    ABoxMqttEc800BridgeHooks hooks = {0};
    ABoxMqttEc800Config config = {0};
    ABoxMqttEc800Buffers buffers = {0};
    static const char *subscriptions[] = {"a/b"};
    uint8_t header[192], topic[128], payload[128], outgoing[128];

    at_port.context = &fixture;
    at_port.tick_ms = tick;
    at_port.write = write_bytes;
    assert(ABoxEc800At_Init(&at, &at_port));
    config.apn = "CMNET";
    config.host = "broker.example";
    config.port = 1883U;
    config.client_id = "device";
    config.username = "user";
    config.password = "secret";
    config.subscriptions = subscriptions;
    config.subscription_count = 1U;
    config.command_timeout_ms = 100U;
    config.open_timeout_ms = 1000U;
    config.connect_timeout_ms = 1000U;
    config.subscribe_timeout_ms = 1000U;
    config.publish_timeout_ms = 1000U;
    config.retry_delay_ms = 100U;
    buffers.header = header; buffers.header_capacity = sizeof(header);
    buffers.topic = topic; buffers.topic_capacity = sizeof(topic);
    buffers.payload = payload; buffers.payload_capacity = sizeof(payload);
    buffers.publish_payload = outgoing; buffers.publish_capacity = sizeof(outgoing);
    hooks.context = &fixture;
    hooks.message = message;
    hooks.network_changed = network_changed;
    hooks.publish_event = publish_event;
    assert(ABoxMqttEc800Bridge_Init(&bridge, &at, &config, &buffers, &hooks,
                                     ABOX_EC800_OWNER_PRODUCT_BASE + 5U, 0U));
    assert(!ABoxMqttEc800Bridge_CanRunBackgroundAt(&bridge));
    memcpy(topic, "a/b", 3U);
    memcpy(payload, "test", 4U);
    bridge.transport.callbacks.message(bridge.transport.callbacks.user,
                                       topic, 3U, payload, 4U);
    assert(fixture.messages == 1U && !strcmp(fixture.topic, "a/b") &&
           !strcmp(fixture.payload, "test"));
    payload[1] = 0;
    bridge.transport.callbacks.message(bridge.transport.callbacks.user,
                                       topic, 3U, payload, 4U);
    assert(fixture.messages == 1U);
    assert(ABoxMqttReceipt_Begin(&bridge.receipt, 17U));
    bridge.transport.callbacks.publish_event(bridge.transport.callbacks.user,
                                             16U, ABOX_MQTT_EC800_PUBLISH_CONFIRMED);
    assert(ABoxMqttReceipt_Get(&bridge.receipt, 17U) == ABOX_MQTT_RECEIPT_PENDING);
    bridge.transport.callbacks.publish_event(bridge.transport.callbacks.user,
                                             17U, ABOX_MQTT_EC800_PUBLISH_CONFIRMED);
    assert(ABoxMqttReceipt_Get(&bridge.receipt, 17U) == ABOX_MQTT_RECEIPT_CONFIRMED);
    assert(bridge.publish_ok == 2U && fixture.confirms == 2U);
    bridge.transport.callbacks.state_changed(bridge.transport.callbacks.user,
                                             ABOX_MQTT_EC800_READY);
    bridge.transport.callbacks.state_changed(bridge.transport.callbacks.user,
                                             ABOX_MQTT_EC800_RETRY_WAIT);
    assert(fixture.network_changes == 1U);
    assert(ABoxMqttReceipt_Begin(&bridge.receipt, 18U));
    ABoxMqttEc800Bridge_OnModemReset(&bridge, 10U);
    assert(ABoxMqttReceipt_Get(&bridge.receipt, 18U) == ABOX_MQTT_RECEIPT_FAILED);
    ABoxMqttEc800Bridge_SetPaused(&bridge, 1U, 10U);
    assert(bridge.paused == 1U);
    return 0;
}
