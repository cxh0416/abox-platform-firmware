#ifndef ABOX_MQTT_V4_H
#define ABOX_MQTT_V4_H

#include <stddef.h>
#include <stdint.h>

#define ABOX_MQTT_V4_VERSION "4.0"
#define ABOX_MQTT_V4_IDEMPOTENCY_GRACE_MS 600000ULL

typedef enum {
    ABOX_MQTT_COMMAND_CORE = 0,
    ABOX_MQTT_COMMAND_QUERY,
    ABOX_MQTT_COMMAND_DISCRETE_ACTION,
    ABOX_MQTT_COMMAND_CONTINUOUS_SESSION,
    ABOX_MQTT_COMMAND_SAFETY_STOP
} ABoxMqttCommandClass;

typedef struct {
    const char *name;
    const char *profile;
    ABoxMqttCommandClass classification;
} ABoxMqttCommandDescriptor;

typedef struct {
    const char *name;
    const char *profile;
    const char *report_type;
} ABoxMqttReportDescriptor;

typedef struct {
    const char *name;
    const char *version;
    const ABoxMqttCommandDescriptor *commands;
    size_t command_count;
    const ABoxMqttReportDescriptor *reports;
    size_t report_count;
} ABoxMqttProfileDescriptor;

/* Products supply a static array containing only their linked profiles. */
typedef struct {
    const ABoxMqttProfileDescriptor *const *profiles;
    size_t count;
} ABoxMqttV4Registry;

const ABoxMqttProfileDescriptor *ABoxMqttV4_FindProfile(
    const ABoxMqttV4Registry *registry, const char *name);
int ABoxMqttV4_RegistryValid(const ABoxMqttV4Registry *registry);
const ABoxMqttCommandDescriptor *ABoxMqttV4_FindCommand(
    const ABoxMqttProfileDescriptor *profile, const char *name);
const ABoxMqttReportDescriptor *ABoxMqttV4_FindReport(
    const ABoxMqttProfileDescriptor *profile, const char *name);

uint8_t ABoxMqttV4_SubscriptionQos(const char *topic);
uint8_t ABoxMqttV4_ReportQos(const char *report_type);
uint64_t ABoxMqttV4_IdempotencyProtectUntil(uint64_t expires_at);

#endif
