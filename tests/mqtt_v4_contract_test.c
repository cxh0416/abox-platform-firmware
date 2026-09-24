#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "abox_mqtt_v4.h"

int main(void)
{
    static const ABoxMqttCommandDescriptor commands[] = {
        {"query", "fixture", ABOX_MQTT_COMMAND_QUERY}
    };
    static const ABoxMqttReportDescriptor reports[] = {
        {"status", "fixture", "state"}
    };
    static const ABoxMqttProfileDescriptor profile = {
        "fixture", "1.0", commands, 1U, reports, 1U
    };
    const ABoxMqttProfileDescriptor *profiles[] = {&profile};
    const ABoxMqttV4Registry registry = {profiles, 1U};

    assert(ABoxMqttV4_RegistryValid(&registry));
    assert(ABoxMqttV4_FindProfile(&registry, "fixture") == &profile);
    assert(ABoxMqttV4_FindProfile(&registry, "missing") == 0);
    assert(ABoxMqttV4_FindCommand(&profile, "query") == commands);
    assert(ABoxMqttV4_FindReport(&profile, "status") == reports);
    assert(ABoxMqttV4_SubscriptionQos("/zxwl/abox/device/request") == 1U);
    assert(ABoxMqttV4_ReportQos("state") == 1U);
    assert(ABoxMqttV4_ReportQos("event") == 1U);
    assert(ABoxMqttV4_ReportQos("telemetry") == 0U);
    assert(ABoxMqttV4_IdempotencyProtectUntil(1770186098000ULL) == 1770186698000ULL);
    assert(ABoxMqttV4_IdempotencyProtectUntil(UINT64_MAX) == UINT64_MAX);
    return 0;
}
