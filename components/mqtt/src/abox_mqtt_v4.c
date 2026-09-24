#include "abox_mqtt_v4.h"

#include <string.h>

int ABoxMqttV4_RegistryValid(const ABoxMqttV4Registry *registry)
{
    size_t i, j, k;
    if (!registry || (registry->count && !registry->profiles)) return 0;
    for (i = 0; i < registry->count; ++i) {
        const ABoxMqttProfileDescriptor *profile = registry->profiles[i];
        if (!profile || !profile->name || !profile->name[0] ||
            !profile->version || !profile->version[0] ||
            (profile->command_count && !profile->commands) ||
            (profile->report_count && !profile->reports)) return 0;
        for (j = 0; j < i; ++j)
            if (strcmp(profile->name, registry->profiles[j]->name) == 0) return 0;
        for (j = 0; j < profile->command_count; ++j) {
            const ABoxMqttCommandDescriptor *command = &profile->commands[j];
            if (!command->name || !command->name[0] || !command->profile ||
                strcmp(command->profile, profile->name) != 0 ||
                command->classification > ABOX_MQTT_COMMAND_SAFETY_STOP) return 0;
            for (k = 0; k < j; ++k)
                if (strcmp(command->name, profile->commands[k].name) == 0) return 0;
        }
        for (j = 0; j < profile->report_count; ++j) {
            const ABoxMqttReportDescriptor *report = &profile->reports[j];
            if (!report->name || !report->name[0] || !report->profile ||
                strcmp(report->profile, profile->name) != 0 ||
                !report->report_type || !report->report_type[0]) return 0;
            for (k = 0; k < j; ++k)
                if (strcmp(report->name, profile->reports[k].name) == 0) return 0;
        }
    }
    return 1;
}

const ABoxMqttProfileDescriptor *ABoxMqttV4_FindProfile(
    const ABoxMqttV4Registry *registry, const char *name)
{
    size_t i;
    if (!registry || !registry->profiles || !name) return 0;
    for (i = 0; i < registry->count; ++i) {
        const ABoxMqttProfileDescriptor *profile = registry->profiles[i];
        if (profile && profile->name && strcmp(profile->name, name) == 0) return profile;
    }
    return 0;
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
