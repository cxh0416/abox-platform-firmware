#ifndef ABOX_CLOUD_SERVICE_H
#define ABOX_CLOUD_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One communication owner calls these functions. The product owns payload
 * encoding and transport; this service owns common ordering and tracking. */
typedef enum {
    ABOX_CLOUD_JOB_NONE = 0,
    ABOX_CLOUD_JOB_MANIFEST,
    ABOX_CLOUD_JOB_HEARTBEAT,
    ABOX_CLOUD_JOB_STATE,
    ABOX_CLOUD_JOB_RESPONSE
} ABoxCloudJobKind;

typedef struct {
    ABoxCloudJobKind kind;
    uint32_t generation;
    const char *reason;       /* For state: startup, reconnect, sync_state, event, periodic. */
    const char *request_id;   /* For response and sync_state; valid until next mutation. */
} ABoxCloudJob;

typedef struct {
    uint32_t heartbeat_period_ms;
    uint32_t status_period_ms;
} ABoxCloudServiceOptions;

typedef struct {
    ABoxCloudServiceOptions options;
    uint32_t generation;
    uint32_t heartbeat_due;
    uint32_t status_due;
    uint32_t event_serial;
    uint32_t in_flight_event_serial;
    uint32_t correlated_serial;
    uint32_t in_flight_correlated_serial;
    uint64_t operation;
    ABoxCloudJobKind in_flight;
    uint8_t ready;
    uint8_t ever_ready;
    uint8_t bootstrap_stage;
    uint8_t response_pending;
    uint8_t response_sync;
    uint8_t sync_pending;
    uint8_t event_pending;
    char response_request_id[65];
    char sync_request_id[65];
    char correlated_reason[32];
    char event_reason[32];
} ABoxCloudService;

int ABoxCloudService_Init(ABoxCloudService *service,
                          const ABoxCloudServiceOptions *options, uint32_t now);
void ABoxCloudService_SetReady(ABoxCloudService *service, int ready, uint32_t now);
void ABoxCloudService_NetworkChanged(ABoxCloudService *service);
int ABoxCloudService_Bootstrapping(const ABoxCloudService *service);
/* The caller retains its response topic/payload until RESPONSE confirmation. */
int ABoxCloudService_QueueResponse(ABoxCloudService *service,
                                   const char *request_id, int sync_state);
/* Queue one full state report correlated to an already accepted product
 * request. Replacing an older pending report matches legacy last-write-wins
 * behavior; an old in-flight receipt cannot clear the replacement. */
int ABoxCloudService_QueueStateReport(ABoxCloudService *service,
                                      const char *request_id, const char *reason);
int ABoxCloudService_WakeState(ABoxCloudService *service, const char *reason);
int ABoxCloudService_Next(ABoxCloudService *service, uint32_t now,
                          ABoxCloudJob *job);
int ABoxCloudService_Begin(ABoxCloudService *service, const ABoxCloudJob *job,
                           uint64_t operation, uint32_t now);
/* Returns the completed job kind only for a matching confirmed operation.
 * A failed operation clears the in-flight slot but retains queued work. */
ABoxCloudJobKind ABoxCloudService_Receipt(ABoxCloudService *service,
                                          uint32_t generation, uint64_t operation,
                                          int confirmed, uint32_t now);

#ifdef __cplusplus
}
#endif
#endif
