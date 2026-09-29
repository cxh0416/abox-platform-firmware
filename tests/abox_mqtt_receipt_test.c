#include "abox_mqtt_receipt.h"
#include <assert.h>

int main(void)
{
    ABoxMqttReceipt receipt;
    ABoxMqttReceipt_Init(&receipt);
    assert(ABoxMqttReceipt_Get(&receipt, 1U) == ABOX_MQTT_RECEIPT_UNKNOWN);
    assert(ABoxMqttReceipt_Begin(&receipt, 41U));
    assert(!ABoxMqttReceipt_Begin(&receipt, 42U));
    ABoxMqttReceipt_OnEvent(&receipt, 42U, ABOX_MQTT_EC800_PUBLISH_CONFIRMED);
    assert(ABoxMqttReceipt_Get(&receipt, 41U) == ABOX_MQTT_RECEIPT_PENDING);
    ABoxMqttReceipt_OnEvent(&receipt, 41U, ABOX_MQTT_EC800_PUBLISH_SUBMITTED);
    assert(ABoxMqttReceipt_Get(&receipt, 41U) == ABOX_MQTT_RECEIPT_PENDING);
    ABoxMqttReceipt_OnEvent(&receipt, 41U, ABOX_MQTT_EC800_PUBLISH_CONFIRMED);
    assert(ABoxMqttReceipt_Get(&receipt, 41U) == ABOX_MQTT_RECEIPT_CONFIRMED);
    assert(ABoxMqttReceipt_Begin(&receipt, 42U));
    ABoxMqttReceipt_OnEvent(&receipt, 41U, ABOX_MQTT_EC800_PUBLISH_FAILED);
    assert(ABoxMqttReceipt_Get(&receipt, 42U) == ABOX_MQTT_RECEIPT_PENDING);
    ABoxMqttReceipt_Invalidate(&receipt);
    assert(ABoxMqttReceipt_Get(&receipt, 42U) == ABOX_MQTT_RECEIPT_FAILED);
    ABoxMqttReceipt_OnEvent(&receipt, 42U, ABOX_MQTT_EC800_PUBLISH_CONFIRMED);
    assert(ABoxMqttReceipt_Get(&receipt, 42U) == ABOX_MQTT_RECEIPT_FAILED);
    ABoxMqttReceipt_Init(&receipt);
    assert(ABoxMqttReceipt_Get(&receipt, 42U) == ABOX_MQTT_RECEIPT_UNKNOWN);
    return 0;
}
