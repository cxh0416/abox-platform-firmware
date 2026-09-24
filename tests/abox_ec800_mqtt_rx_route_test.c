#include <assert.h>
#include <string.h>

#include "abox_ec800_at.h"
#include "abox_mqtt_ec800_rx.h"

typedef struct {
    uint32_t now;
    unsigned messages, command_done, ota_bytes;
    uint8_t payload[32];
    size_t payload_length;
} Fixture;

static uint32_t tick(void *user) { return ((Fixture *)user)->now; }
static int write_at(void *user, const uint8_t *data, uint16_t length, uint32_t timeout)
{ (void)user; (void)data; (void)length; (void)timeout; return 1; }
static void command_done(ABoxEc800Result result, void *user)
{ assert(result == ABOX_EC800_RESULT_OK); ++((Fixture *)user)->command_done; }
static void message(void *user, const uint8_t *topic, size_t topic_length,
                    const uint8_t *payload, size_t payload_length)
{
    Fixture *f = user;
    assert(topic_length == 5U && memcmp(topic, "topic", 5U) == 0);
    assert(payload_length <= sizeof(f->payload));
    memcpy(f->payload, payload, payload_length);
    f->payload_length = payload_length;
    ++f->messages;
}
static void ota_event(ABoxEc800Event event, const uint8_t *data,
                      uint16_t length, void *user)
{ (void)data; if (event == ABOX_EC800_EVENT_RAW) ((Fixture *)user)->ota_bytes += length; }
static void feed_rx(void *context, const uint8_t *data, uint16_t length, uint32_t now)
{ ABoxMqttEc800Rx_Feed(context, data, length, now); }
static void poll_rx(void *context, uint32_t now)
{ ABoxMqttEc800Rx_Poll(context, now); }
static int collecting_rx(void *context)
{ return ABoxMqttEc800Rx_IsCollecting(context); }
static int locked_rx(void *context)
{ return ABoxMqttEc800Rx_IsLocked(context); }

int main(void)
{
    Fixture f = {0};
    ABoxEc800At at;
    ABoxEc800AtPort at_port = {0};
    ABoxEc800MqttRxPort rx_port = {0};
    ABoxMqttEc800Rx rx;
    uint8_t header[96], topic[32], payload[32];
    static const uint8_t incoming[] = "+QMTRECV: 0,0,\"topic\",6,a\nOK\nb";
    static const uint8_t raw[] = "+QMTRECV: 0,0,\"topic\",1,x";
    static const uint8_t partial[] = "+QMTRECV: 0,0,\"topic\",3,x";
    at_port.context = &f; at_port.tick_ms = tick; at_port.write = write_at;
    assert(ABoxEc800At_Init(&at, &at_port));
    assert(ABoxMqttEc800Rx_Init(&rx, header, sizeof(header), topic, sizeof(topic),
                                payload, sizeof(payload), 100U, 0U, message, &f));
    rx_port.context = &rx; rx_port.feed = feed_rx; rx_port.poll = poll_rx;
    rx_port.collecting = collecting_rx; rx_port.locked = locked_rx;
    assert(ABoxEc800At_SetMqttReceiver(&at, &rx_port));
    assert(ABoxEc800At_Register(&at, ABOX_EC800_OWNER_OTA, ota_event, &f));
    assert(ABoxEc800At_Submit(&at, "AT+CSQ", ABOX_EC800_OWNER_PRODUCT_BASE,
                             ABOX_EC800_PRIORITY_NORMAL, 500U, command_done, &f));
    ABoxEc800At_Task(&at);
    ABoxEc800At_Feed(&at, incoming, (uint16_t)(sizeof(incoming) - 1U));
    assert(f.messages == 1U && f.payload_length == 6U);
    assert(memcmp(f.payload, "a\nOK\nb", 6U) == 0);
    assert(f.command_done == 0U && at.active_valid);
    ABoxEc800At_Feed(&at, (const uint8_t *)"OK\r\n", 4U);
    assert(f.command_done == 1U);

    assert(ABoxEc800At_BeginRaw(&at, ABOX_EC800_OWNER_OTA, sizeof(raw) - 1U));
    ABoxEc800At_Feed(&at, raw, (uint16_t)(sizeof(raw) - 1U));
    assert(f.ota_bytes == sizeof(raw) - 1U && f.messages == 1U);

    ABoxEc800At_Feed(&at, partial, (uint16_t)(sizeof(partial) - 1U));
    f.now = 101U;
    ABoxEc800At_Task(&at);
    assert(at.quarantined && ABoxMqttEc800Rx_IsLocked(&rx));
    ABoxMqttEc800Rx_Reset(&rx);
    ABoxEc800At_Reset(&at);
    assert(!at.quarantined);
    return 0;
}
