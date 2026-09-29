#ifndef ABOX_MQTT_EC800_BRIDGE_H
#define ABOX_MQTT_EC800_BRIDGE_H

#include "abox_ec800_iccid.h"
#include "abox_mqtt_ec800.h"
#include "abox_mqtt_receipt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A single-owner App bridge for the standard EC800 transport. The caller owns
 * config strings and all RX/TX buffers for the lifetime of the session. */
typedef struct {
    void *context;
    void (*message)(void *context, const char *topic, const char *payload);
    void (*network_changed)(void *context);
    void (*publish_event)(void *context, uint64_t operation,
                          ABoxMqttEc800PublishEvent event);
    void (*state_changed)(void *context, ABoxMqttEc800State state);
    void (*modem_reset)(void *context);
} ABoxMqttEc800BridgeHooks;

typedef struct {
    ABoxMqttEc800 transport;
    ABoxMqttReceipt receipt;
    ABoxEc800Iccid iccid;
    ABoxEc800At *at;
    ABoxMqttEc800BridgeHooks hooks;
    ABoxMqttEc800State last_state;
    uint32_t publish_ok;
    uint8_t initialized, paused, security_ready;
} ABoxMqttEc800Bridge;

int ABoxMqttEc800Bridge_Init(ABoxMqttEc800Bridge *bridge, ABoxEc800At *at,
                             const ABoxMqttEc800Config *config,
                             const ABoxMqttEc800Buffers *buffers,
                             const ABoxMqttEc800BridgeHooks *hooks,
                             ABoxEc800Owner iccid_owner, uint32_t now_ms);
void ABoxMqttEc800Bridge_Poll(ABoxMqttEc800Bridge *bridge, uint32_t now_ms);
void ABoxMqttEc800Bridge_OnModemReset(ABoxMqttEc800Bridge *bridge,
                                       uint32_t now_ms);
void ABoxMqttEc800Bridge_SetPaused(ABoxMqttEc800Bridge *bridge,
                                    uint8_t paused, uint32_t now_ms);
void ABoxMqttEc800Bridge_SetSecurityReady(ABoxMqttEc800Bridge *bridge,
                                           uint8_t ready);
int ABoxMqttEc800Bridge_Configure(ABoxMqttEc800Bridge *bridge,
                                   const ABoxMqttEc800Config *config);
int ABoxMqttEc800Bridge_Publish(ABoxMqttEc800Bridge *bridge,
                                 const char *topic, const char *payload,
                                 uint8_t qos, uint8_t retain,
                                 uint64_t *operation);
void ABoxMqttEc800Bridge_CancelPublish(ABoxMqttEc800Bridge *bridge);
int ABoxMqttEc800Bridge_CanRunBackgroundAt(const ABoxMqttEc800Bridge *bridge);
int ABoxMqttEc800Bridge_RequestIccid(ABoxMqttEc800Bridge *bridge,
                                       uint32_t now_ms);
int ABoxMqttEc800Bridge_BorrowWorkspace(ABoxMqttEc800Bridge *bridge,
                                         size_t minimum_capacity);
void ABoxMqttEc800Bridge_ReturnWorkspace(ABoxMqttEc800Bridge *bridge);

#ifdef __cplusplus
}
#endif
#endif
