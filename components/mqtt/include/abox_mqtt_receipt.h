#ifndef ABOX_MQTT_RECEIPT_H
#define ABOX_MQTT_RECEIPT_H

#include <stdint.h>
#include "abox_mqtt_ec800.h"

typedef enum {
    ABOX_MQTT_RECEIPT_UNKNOWN = 0,
    ABOX_MQTT_RECEIPT_PENDING,
    ABOX_MQTT_RECEIPT_CONFIRMED,
    ABOX_MQTT_RECEIPT_FAILED
} ABoxMqttReceiptStatus;

/* One in-flight publish per ABoxMqttEc800. Call from the same service owner. */
typedef struct {
    uint64_t operation;
    ABoxMqttReceiptStatus status;
} ABoxMqttReceipt;

void ABoxMqttReceipt_Init(ABoxMqttReceipt *receipt);
int ABoxMqttReceipt_Begin(ABoxMqttReceipt *receipt, uint64_t operation);
void ABoxMqttReceipt_OnEvent(ABoxMqttReceipt *receipt, uint64_t operation,
                             ABoxMqttEc800PublishEvent event);
void ABoxMqttReceipt_Invalidate(ABoxMqttReceipt *receipt);
ABoxMqttReceiptStatus ABoxMqttReceipt_Get(const ABoxMqttReceipt *receipt,
                                          uint64_t operation);

#endif
