#include "abox_boot_v2_ec800_port.h"

#include <assert.h>
#include <string.h>

static char command[128];
static ABoxBootV2AtResult completion;
static unsigned completed;
static unsigned lines;

static uint32_t tick(void *context) { (void)context; return 123U; }
static int write_bytes(void *context, const uint8_t *data, uint16_t length,
                       uint32_t timeout_ms)
{
    (void)context;
    (void)timeout_ms;
    assert(length < sizeof(command));
    memcpy(command, data, length);
    command[length] = 0;
    return 1;
}
static void done(ABoxBootV2AtResult result, void *user)
{
    assert(user == &completed);
    completion = result;
    ++completed;
}
static void event(ABoxBootV2AtEvent kind, const uint8_t *data,
                  uint16_t length, void *user)
{
    assert(user == &lines);
    assert(kind == ABOX_BOOT_V2_AT_LINE);
    if (length == 9U && memcmp(data, "+QHTTP: 1", 9U) == 0)
        ++lines;
}
static int product_stop(void *context, uint32_t now)
{
    (void)context;
    return now == 123U;
}

int main(void)
{
    ABoxEc800At at;
    ABoxEc800AtPort at_port = {0, tick, write_bytes, 0};
    ABoxBootV2AppPort port = {0};
    ABoxBootV2Ec800Adapter adapter;
    const uint8_t reply[] = "OK\r\n+QHTTPGET: 0,200,16\r\n";
    const uint8_t urc[] = "+QHTTP: 1\r\n";

    assert(ABoxEc800At_Init(&at, &at_port));
    port.tick_ms = tick;
    port.mqtt_stop = product_stop;
    assert(!ABoxBootV2Ec800Adapter_Bind(0, &at, &port));
    assert(ABoxBootV2Ec800Adapter_Bind(&adapter, &at, &port));
    assert(port.context == &adapter && port.tick_ms == tick &&
           port.mqtt_stop == product_stop);
    assert(port.mqtt_stop(port.context, 123U));
    port.register_events(port.context, event, &lines);
    assert(port.submit(port.context, "AT+QHTTPGET=80", 1000U,
                       done, &completed));
    assert(port.has_pending(port.context));
    assert(!port.submit(port.context, "AT+QHTTPGET=90", 1000U,
                        done, &completed));
    ABoxEc800At_Task(&at);
    assert(strstr(command, "AT+QHTTPGET=80") != 0);
    assert(port.is_active(port.context));
    ABoxEc800At_Feed(&at, reply, sizeof(reply) - 1U);
    assert(completed == 1U && completion == ABOX_BOOT_V2_AT_OK);
    assert(!port.is_active(port.context));
    ABoxEc800At_Feed(&at, urc, sizeof(urc) - 1U);
    assert(lines == 1U);
    ABoxEc800At_SetRxOverflowCount(&at, 7U);
    assert(port.rx_overflow_count(port.context) == 7U);
    assert(port.submit(port.context, "AT+QHTTPGET=90", 1000U,
                       done, &completed));
    port.cancel(port.context);
    assert(!port.has_pending(port.context));
    assert(completed == 1U);
    return 0;
}
