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

const ABoxMqttProfileDescriptor *ABoxMqttV4_LockerProfile(void);
const ABoxMqttProfileDescriptor *ABoxMqttV4_VehicleChassisProfile(void);
const ABoxMqttProfileDescriptor *ABoxMqttV4_SweeperVehicleProfile(void);
const ABoxMqttCommandDescriptor *ABoxMqttV4_FindCommand(
    const ABoxMqttProfileDescriptor *profile, const char *name);
const ABoxMqttReportDescriptor *ABoxMqttV4_FindReport(
    const ABoxMqttProfileDescriptor *profile, const char *name);

uint8_t ABoxMqttV4_SubscriptionQos(const char *topic);
uint8_t ABoxMqttV4_ReportQos(const char *report_type);
uint64_t ABoxMqttV4_IdempotencyProtectUntil(uint64_t expires_at);

/* Registry normalization for locker grid-set commands. grid_nos are numeric
 * strings and are sorted numerically without modifying the caller's array. */
int ABoxMqttV4_CanonicalizeLockerRequest(char *output, size_t output_size,
                                         const char *command, uint64_t expires_at,
                                         uint8_t include_expires_at,
                                         const char *const *grid_nos, size_t grid_count);

#endif
