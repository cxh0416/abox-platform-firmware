#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "abox_mqtt_v4.h"

int main(void)
{
    const char *grids[] = {"6", "1", "2"};
    const char *all[] = {"all"};
    const char *leading_zero[] = {"01"};
    const ABoxMqttProfileDescriptor *profile = ABoxMqttV4_LockerProfile();
    char canonical[192];

    assert(profile != 0);
    assert(strcmp(profile->name, "locker") == 0);
    assert(ABoxMqttV4_FindCommand(profile, "open_locker")->classification ==
           ABOX_MQTT_COMMAND_DISCRETE_ACTION);
    assert(strcmp(ABoxMqttV4_FindReport(profile, "locker_status")->report_type, "state") == 0);
    profile = ABoxMqttV4_VehicleChassisProfile();
    assert(profile != 0);
    assert(strcmp(profile->name, "vehicle_chassis") == 0);
    assert(ABoxMqttV4_FindCommand(profile, "power_on")->classification ==
           ABOX_MQTT_COMMAND_DISCRETE_ACTION);
    assert(ABoxMqttV4_FindCommand(profile, "power_off")->classification ==
           ABOX_MQTT_COMMAND_SAFETY_STOP);
    assert(ABoxMqttV4_FindCommand(profile, "chassis_control")->classification ==
           ABOX_MQTT_COMMAND_CONTINUOUS_SESSION);
    assert(strcmp(ABoxMqttV4_FindReport(profile, "chassis_status")->report_type, "state") == 0);
    profile = ABoxMqttV4_SweeperVehicleProfile();
    assert(profile != 0);
    assert(strcmp(profile->name, "sweeper_vehicle") == 0);
    assert(strcmp(profile->version, "1.0") == 0);
    assert(ABoxMqttV4_FindCommand(profile, "power_on")->classification ==
           ABOX_MQTT_COMMAND_DISCRETE_ACTION);
    assert(ABoxMqttV4_FindCommand(profile, "power_off")->classification ==
           ABOX_MQTT_COMMAND_SAFETY_STOP);
    assert(strcmp(ABoxMqttV4_FindReport(profile, "sweeper_status")->report_type, "state") == 0);
    assert(ABoxMqttV4_SubscriptionQos("/zxwl/abox/device/request") == 1U);
    assert(ABoxMqttV4_ReportQos("state") == 1U);
    assert(ABoxMqttV4_ReportQos("event") == 1U);
    assert(ABoxMqttV4_ReportQos("telemetry") == 0U);
    assert(ABoxMqttV4_IdempotencyProtectUntil(1770186098000ULL) == 1770186698000ULL);
    assert(ABoxMqttV4_CanonicalizeLockerRequest(canonical, sizeof(canonical),
        "open_locker", 1770186098000ULL, 1U, grids, 3U));
    assert(strcmp(canonical,
        "{\"command\":\"open_locker\",\"expiresAt\":1770186098000,\"params\":{\"gridNos\":[\"1\",\"2\",\"6\"]}}") == 0);
    assert(ABoxMqttV4_CanonicalizeLockerRequest(canonical, sizeof(canonical),
        "check_locker_status", 0U, 0U, all, 1U));
    assert(strcmp(canonical,
        "{\"command\":\"check_locker_status\",\"params\":{\"gridNos\":[\"all\"]}}") == 0);
    assert(!ABoxMqttV4_CanonicalizeLockerRequest(canonical, sizeof(canonical),
        "open_locker", 1770186098000ULL, 1U, leading_zero, 1U));
    return 0;
}
