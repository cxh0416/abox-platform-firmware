#include "abox_boot_v2_services.h"
static uint32_t tick(void *context)
{
    ABoxEc800At *at = ((ABoxBootV2Ec800Adapter *)context)->at;
    return at->port.tick_ms(at->port.context);
}
static ABoxMqttEc800Bridge *bridge(void *context)
{ return (ABoxMqttEc800Bridge *)((ABoxBootV2Ec800Adapter *)context)->mqtt_bridge; }
static void pause(void *context, uint8_t paused)
{ ABoxMqttEc800Bridge_SetPaused(bridge(context), paused, tick(context)); }
static int stop(void *context, uint32_t now)
{ return ABoxMqttEc800_Stop(&bridge(context)->transport, now); }
static int borrow(void *context)
{ return ABoxMqttEc800Bridge_BorrowWorkspace(bridge(context), ABOX_BOOT_V2_APP_RAW_CHUNK); }
static void release(void *context)
{ ABoxMqttEc800Bridge_ReturnWorkspace(bridge(context)); }
static uint8_t client(void *context)
{ return bridge(context)->transport.config.client_index; }
int ABoxBootV2Ec800Adapter_BindServices(ABoxBootV2Ec800Adapter *adapter,
    ABoxEc800At *at, ABoxMqttEc800Bridge *mqtt, ABoxBootV2AppPort *port)
{
    if (!adapter || !at || !at->port.tick_ms || !mqtt || !mqtt->initialized ||
        mqtt->at != at || !port ||
        (!port->transfer_buffer && mqtt->transport.buffers.payload_capacity < ABOX_BOOT_V2_APP_RAW_CHUNK))
        return 0;
    if (!ABoxBootV2Ec800Adapter_Bind(adapter, at, port)) return 0;
    adapter->mqtt_bridge = mqtt;
    port->tick_ms = tick;
    port->mqtt_pause = pause;
    port->mqtt_stop = stop;
    port->workspace_borrow = borrow;
    port->workspace_return = release;
    port->mqtt_client_index = client;
    if (!port->transfer_buffer) {
        port->transfer_buffer = mqtt->transport.buffers.payload;
        port->transfer_buffer_size = (uint32_t)mqtt->transport.buffers.payload_capacity;
    }
    return 1;
}
