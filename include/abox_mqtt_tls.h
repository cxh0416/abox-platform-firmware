#ifndef ABOX_MQTT_TLS_H
#define ABOX_MQTT_TLS_H
#include "abox_ec800_tls.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    ABoxEc800Tls *tls;
    ABoxEc800CommandPort port;
    ABoxTlsLease lease;
    uint64_t operation;
    uint32_t started, timeout;
    uint8_t client, enabled, pinned, waiting;
    ABoxAsyncStatus status;
} ABoxMqttTls;
/* Exactly one instance per modem/client. The caller supplies a firmware-verified
 * client index, serializes calls and confirms QMT connection closure before
 * Start/Unbind. This module does not connect, subscribe, or publish. */
int ABoxMqttTls_Init(ABoxMqttTls *mqtt, ABoxEc800Tls *tls,
                     const ABoxEc800CommandPort *port, uint8_t client);
int ABoxMqttTls_Start(ABoxMqttTls *mqtt, int enabled, ABoxTlsLease lease,
                      uint32_t now, uint32_t timeout);
/* After confirmed disconnect: explicitly issue SSL=0; retain the pin until OK. */
int ABoxMqttTls_Unbind(ABoxMqttTls *mqtt, uint32_t now, uint32_t timeout);
void ABoxMqttTls_Poll(ABoxMqttTls *mqtt, uint32_t now);
void ABoxMqttTls_Cancel(ABoxMqttTls *mqtt);
/* Call after modem reset, transport drain, and TLS manager reset. */
void ABoxMqttTls_OnModemReset(ABoxMqttTls *mqtt);
#ifdef __cplusplus
}
#endif
#endif
