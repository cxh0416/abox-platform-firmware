#include "abox_mqtt_v4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const ABoxMqttCommandDescriptor locker_commands[] = {
    {"open_locker", "locker", ABOX_MQTT_COMMAND_DISCRETE_ACTION},
    {"check_locker_status", "locker", ABOX_MQTT_COMMAND_QUERY}
};

static const ABoxMqttReportDescriptor locker_reports[] = {
    {"locker_status", "locker", "state"}
};

static const ABoxMqttProfileDescriptor locker_profile = {
    "locker", "1.0",
    locker_commands, sizeof(locker_commands) / sizeof(locker_commands[0]),
    locker_reports, sizeof(locker_reports) / sizeof(locker_reports[0])
};

static const ABoxMqttCommandDescriptor vehicle_chassis_commands[] = {
    {"power_on", "vehicle_chassis", ABOX_MQTT_COMMAND_DISCRETE_ACTION},
    {"power_off", "vehicle_chassis", ABOX_MQTT_COMMAND_SAFETY_STOP},
    {"chassis_control", "vehicle_chassis", ABOX_MQTT_COMMAND_CONTINUOUS_SESSION}
};

static const ABoxMqttReportDescriptor vehicle_chassis_reports[] = {
    {"chassis_status", "vehicle_chassis", "state"}
};

static const ABoxMqttProfileDescriptor vehicle_chassis_profile = {
    "vehicle_chassis", "1.0",
    vehicle_chassis_commands,
    sizeof(vehicle_chassis_commands) / sizeof(vehicle_chassis_commands[0]),
    vehicle_chassis_reports,
    sizeof(vehicle_chassis_reports) / sizeof(vehicle_chassis_reports[0])
};

static const ABoxMqttCommandDescriptor sweeper_vehicle_commands[] = {
    {"power_on", "sweeper_vehicle", ABOX_MQTT_COMMAND_DISCRETE_ACTION},
    {"power_off", "sweeper_vehicle", ABOX_MQTT_COMMAND_SAFETY_STOP}
};

static const ABoxMqttReportDescriptor sweeper_vehicle_reports[] = {
    {"sweeper_status", "sweeper_vehicle", "state"}
};

static const ABoxMqttProfileDescriptor sweeper_vehicle_profile = {
    "sweeper_vehicle", "1.0",
    sweeper_vehicle_commands,
    sizeof(sweeper_vehicle_commands) / sizeof(sweeper_vehicle_commands[0]),
    sweeper_vehicle_reports,
    sizeof(sweeper_vehicle_reports) / sizeof(sweeper_vehicle_reports[0])
};

const ABoxMqttProfileDescriptor *ABoxMqttV4_LockerProfile(void)
{
    return &locker_profile;
}

const ABoxMqttProfileDescriptor *ABoxMqttV4_VehicleChassisProfile(void)
{
    return &vehicle_chassis_profile;
}

const ABoxMqttProfileDescriptor *ABoxMqttV4_SweeperVehicleProfile(void)
{
    return &sweeper_vehicle_profile;
}

const ABoxMqttCommandDescriptor *ABoxMqttV4_FindCommand(
    const ABoxMqttProfileDescriptor *profile, const char *name)
{
    size_t i;
    if (!profile || !name) return 0;
    for (i = 0; i < profile->command_count; ++i)
        if (strcmp(profile->commands[i].name, name) == 0) return &profile->commands[i];
    return 0;
}

const ABoxMqttReportDescriptor *ABoxMqttV4_FindReport(
    const ABoxMqttProfileDescriptor *profile, const char *name)
{
    size_t i;
    if (!profile || !name) return 0;
    for (i = 0; i < profile->report_count; ++i)
        if (strcmp(profile->reports[i].name, name) == 0) return &profile->reports[i];
    return 0;
}

uint8_t ABoxMqttV4_SubscriptionQos(const char *topic)
{
    (void)topic;
    return 1U;
}

uint8_t ABoxMqttV4_ReportQos(const char *report_type)
{
    return (report_type && strcmp(report_type, "telemetry") == 0) ? 0U : 1U;
}

uint64_t ABoxMqttV4_IdempotencyProtectUntil(uint64_t expires_at)
{
    if (UINT64_MAX - expires_at < ABOX_MQTT_V4_IDEMPOTENCY_GRACE_MS) return UINT64_MAX;
    return expires_at + ABOX_MQTT_V4_IDEMPOTENCY_GRACE_MS;
}

static int parse_grid_no(const char *text, unsigned long *value)
{
    char *end;
    const char *cursor;
    unsigned long parsed;
    if (!text || !text[0] || !value) return 0;
    if (text[0] < '1' || text[0] > '9') return 0;
    for (cursor = text; *cursor; ++cursor)
        if (*cursor < '0' || *cursor > '9') return 0;
    parsed = strtoul(text, &end, 10);
    if (*end != '\0' || parsed == 0UL || parsed > 65535UL) return 0;
    *value = parsed;
    return 1;
}

int ABoxMqttV4_CanonicalizeLockerRequest(char *output, size_t output_size,
                                         const char *command, uint64_t expires_at,
                                         uint8_t include_expires_at,
                                         const char *const *grid_nos, size_t grid_count)
{
    unsigned long values[32];
    size_t i, j, used;
    int written;
    if (!output || output_size == 0U || !command || !grid_nos ||
        grid_count == 0U || grid_count > 32U) return 0;
    output[0] = '\0';
    if (strcmp(command, "open_locker") != 0 && strcmp(command, "check_locker_status") != 0) return 0;
    if (grid_count == 1U && strcmp(grid_nos[0], "all") == 0) {
        written = include_expires_at
            ? snprintf(output, output_size,
                "{\"command\":\"%s\",\"expiresAt\":%llu,\"params\":{\"gridNos\":[\"all\"]}}",
                command, (unsigned long long)expires_at)
            : snprintf(output, output_size,
                "{\"command\":\"%s\",\"params\":{\"gridNos\":[\"all\"]}}", command);
        return written >= 0 && (size_t)written < output_size;
    }
    for (i = 0; i < grid_count; ++i) {
        if (!parse_grid_no(grid_nos[i], &values[i])) return 0;
        for (j = 0; j < i; ++j) if (values[j] == values[i]) return 0;
    }
    for (i = 1; i < grid_count; ++i) {
        unsigned long value = values[i];
        j = i;
        while (j > 0U && values[j - 1U] > value) { values[j] = values[j - 1U]; --j; }
        values[j] = value;
    }
    written = include_expires_at
        ? snprintf(output, output_size,
            "{\"command\":\"%s\",\"expiresAt\":%llu,\"params\":{\"gridNos\":[",
            command, (unsigned long long)expires_at)
        : snprintf(output, output_size,
            "{\"command\":\"%s\",\"params\":{\"gridNos\":[", command);
    if (written < 0 || (size_t)written >= output_size) return 0;
    used = (size_t)written;
    for (i = 0; i < grid_count; ++i) {
        written = snprintf(output + used, output_size - used, "%s\"%lu\"", i ? "," : "", values[i]);
        if (written < 0 || (size_t)written >= output_size - used) return 0;
        used += (size_t)written;
    }
    written = snprintf(output + used, output_size - used, "]}}");
    return written >= 0 && (size_t)written < output_size - used;
}
