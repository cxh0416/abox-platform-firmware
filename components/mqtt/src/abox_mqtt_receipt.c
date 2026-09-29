#include "abox_mqtt_receipt.h"

void ABoxMqttReceipt_Init(ABoxMqttReceipt *receipt)
{
    if (!receipt) return;
    receipt->operation = 0U;
    receipt->status = ABOX_MQTT_RECEIPT_UNKNOWN;
}

int ABoxMqttReceipt_Begin(ABoxMqttReceipt *receipt, uint64_t operation)
{
    if (!receipt || !operation || receipt->status == ABOX_MQTT_RECEIPT_PENDING ||
        operation <= receipt->operation)
        return 0;
    receipt->operation = operation;
    receipt->status = ABOX_MQTT_RECEIPT_PENDING;
    return 1;
}

void ABoxMqttReceipt_OnEvent(ABoxMqttReceipt *receipt, uint64_t operation,
                             ABoxMqttEc800PublishEvent event)
{
    if (!receipt || operation != receipt->operation ||
        receipt->status != ABOX_MQTT_RECEIPT_PENDING) return;
    if (event == ABOX_MQTT_EC800_PUBLISH_CONFIRMED)
        receipt->status = ABOX_MQTT_RECEIPT_CONFIRMED;
    else if (event == ABOX_MQTT_EC800_PUBLISH_FAILED)
        receipt->status = ABOX_MQTT_RECEIPT_FAILED;
}

void ABoxMqttReceipt_Invalidate(ABoxMqttReceipt *receipt)
{
    if (receipt && receipt->status == ABOX_MQTT_RECEIPT_PENDING)
        receipt->status = ABOX_MQTT_RECEIPT_FAILED;
}

ABoxMqttReceiptStatus ABoxMqttReceipt_Get(const ABoxMqttReceipt *receipt,
                                          uint64_t operation)
{
    if (!receipt || !operation || operation != receipt->operation)
        return ABOX_MQTT_RECEIPT_UNKNOWN;
    return receipt->status;
}
