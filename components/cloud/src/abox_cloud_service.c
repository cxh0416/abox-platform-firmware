#include "abox_cloud_service.h"

#include <string.h>

static int copy_text(char *to, unsigned capacity, const char *from)
{
    unsigned length;
    if (!to || !from || !capacity) return 0;
    for (length = 0U; from[length] != '\0'; ++length)
        if (length + 1U >= capacity) return 0;
    memcpy(to, from, length + 1U);
    return 1;
}

int ABoxCloudService_Init(ABoxCloudService *service,
                          const ABoxCloudServiceOptions *options, uint32_t now)
{
    if (!service || !options || !options->heartbeat_period_ms ||
        options->heartbeat_period_ms > INT32_MAX ||
        options->status_period_ms > INT32_MAX) return 0;
    memset(service, 0, sizeof(*service));
    service->options = *options;
    service->generation = 1U;
    service->heartbeat_due = now;
    service->status_due = now;
    return 1;
}

void ABoxCloudService_NetworkChanged(ABoxCloudService *service)
{
    if (!service) return;
    service->generation++;
    if (!service->generation) service->generation = 1U;
    service->ready = 0U;
    service->bootstrap_stage = 0U;
    service->in_flight = ABOX_CLOUD_JOB_NONE;
    service->operation = 0U;
    /* A confirmed sync response remains confirmed; its correlated report
     * resumes after the new connection's complete bootstrap state. */
}

void ABoxCloudService_SetReady(ABoxCloudService *service, int ready, uint32_t now)
{
    if (!service) return;
    if (!ready) {
        if (service->ready) ABoxCloudService_NetworkChanged(service);
        return;
    }
    if (service->ready) return;
    service->ready = 1U;
    service->bootstrap_stage = 1U;
    service->heartbeat_due = now;
    service->status_due = now;
}

int ABoxCloudService_Bootstrapping(const ABoxCloudService *service)
{
    return service && service->ready && service->bootstrap_stage != 0U;
}

int ABoxCloudService_QueueResponse(ABoxCloudService *service,
                                   const char *request_id, int sync_state)
{
    if (!service || service->response_pending || service->sync_pending ||
        !request_id || !request_id[0] ||
        !copy_text(service->response_request_id,
                   sizeof(service->response_request_id), request_id)) return 0;
    service->response_pending = 1U;
    service->response_sync = sync_state ? 1U : 0U;
    return 1;
}

int ABoxCloudService_QueueStateReport(ABoxCloudService *service,
                                      const char *request_id, const char *reason)
{
    char id[sizeof(service->sync_request_id)];
    char why[sizeof(service->correlated_reason)];
    if (!service || !request_id || !request_id[0] || !reason || !reason[0] ||
        !copy_text(id, sizeof(id), request_id) ||
        !copy_text(why, sizeof(why), reason)) return 0;
    (void)copy_text(service->sync_request_id,
                    sizeof(service->sync_request_id), id);
    (void)copy_text(service->correlated_reason,
                    sizeof(service->correlated_reason), why);
    service->sync_pending = 1U;
    if (++service->correlated_serial == 0U) ++service->correlated_serial;
    return 1;
}

int ABoxCloudService_WakeState(ABoxCloudService *service, const char *reason)
{
    if (!service || !reason || !reason[0] ||
        !copy_text(service->event_reason, sizeof(service->event_reason), reason))
        return 0;
    service->event_pending = 1U;
    service->event_serial++;
    return 1;
}

int ABoxCloudService_Next(ABoxCloudService *service, uint32_t now,
                          ABoxCloudJob *job)
{
    if (!service || !job) return 0;
    memset(job, 0, sizeof(*job));
    if (!service->ready || service->in_flight != ABOX_CLOUD_JOB_NONE) return 0;
    job->generation = service->generation;
    if (service->bootstrap_stage == 1U) job->kind = ABOX_CLOUD_JOB_MANIFEST;
    else if (service->bootstrap_stage == 2U) job->kind = ABOX_CLOUD_JOB_HEARTBEAT;
    else if (service->bootstrap_stage == 3U) {
        job->kind = ABOX_CLOUD_JOB_STATE;
        job->reason = service->ever_ready ? "reconnect" : "startup";
    } else if (service->response_pending) {
        job->kind = ABOX_CLOUD_JOB_RESPONSE;
        job->request_id = service->response_request_id;
    } else if (service->sync_pending) {
        job->kind = ABOX_CLOUD_JOB_STATE;
        job->reason = service->correlated_reason[0] ?
            service->correlated_reason : "sync_state";
        job->request_id = service->sync_request_id;
    } else if (service->event_pending) {
        job->kind = ABOX_CLOUD_JOB_STATE;
        job->reason = service->event_reason;
    } else if ((int32_t)(now - service->heartbeat_due) >= 0) {
        job->kind = ABOX_CLOUD_JOB_HEARTBEAT;
    } else if (service->options.status_period_ms &&
               (int32_t)(now - service->status_due) >= 0) {
        job->kind = ABOX_CLOUD_JOB_STATE;
        job->reason = "periodic";
    } else return 0;
    return 1;
}

int ABoxCloudService_Begin(ABoxCloudService *service, const ABoxCloudJob *job,
                           uint64_t operation, uint32_t now)
{
    ABoxCloudJob expected;
    if (!service || !job || !operation || service->in_flight != ABOX_CLOUD_JOB_NONE ||
        !ABoxCloudService_Next(service, now, &expected) ||
        job->kind != expected.kind || job->generation != expected.generation ||
        ((job->request_id || expected.request_id) &&
         (!job->request_id || !expected.request_id ||
          strcmp(job->request_id, expected.request_id) != 0)) ||
        ((job->reason || expected.reason) &&
         (!job->reason || !expected.reason ||
          strcmp(job->reason, expected.reason) != 0)))
        return 0;
    service->in_flight = job->kind;
    service->operation = operation;
    service->in_flight_event_serial =
        job->kind == ABOX_CLOUD_JOB_STATE && service->event_pending &&
        !service->sync_pending && service->bootstrap_stage == 0U
            ? service->event_serial : 0U;
    service->in_flight_correlated_serial =
        job->kind == ABOX_CLOUD_JOB_STATE && service->sync_pending &&
        service->bootstrap_stage == 0U ? service->correlated_serial : 0U;
    return 1;
}

ABoxCloudJobKind ABoxCloudService_Receipt(ABoxCloudService *service,
                                          uint32_t generation, uint64_t operation,
                                          int confirmed, uint32_t now)
{
    ABoxCloudJobKind kind;
    if (!service || !operation || generation != service->generation ||
        operation != service->operation || service->in_flight == ABOX_CLOUD_JOB_NONE)
        return ABOX_CLOUD_JOB_NONE;
    kind = service->in_flight;
    service->in_flight = ABOX_CLOUD_JOB_NONE;
    service->operation = 0U;
    if (!confirmed) return ABOX_CLOUD_JOB_NONE;
    if (kind == ABOX_CLOUD_JOB_MANIFEST && service->bootstrap_stage == 1U)
        service->bootstrap_stage = 2U;
    else if (kind == ABOX_CLOUD_JOB_HEARTBEAT && service->bootstrap_stage == 2U)
        service->bootstrap_stage = 3U;
    else if (kind == ABOX_CLOUD_JOB_STATE && service->bootstrap_stage == 3U) {
        service->bootstrap_stage = 0U;
        service->ever_ready = 1U;
        service->heartbeat_due = now + service->options.heartbeat_period_ms;
        service->status_due = now + service->options.status_period_ms;
    } else if (kind == ABOX_CLOUD_JOB_RESPONSE && service->response_pending) {
        service->response_pending = 0U;
        if (service->response_sync) {
            (void)copy_text(service->sync_request_id,
                            sizeof(service->sync_request_id),
                            service->response_request_id);
            (void)copy_text(service->correlated_reason,
                            sizeof(service->correlated_reason), "sync_state");
            service->sync_pending = 1U;
            if (++service->correlated_serial == 0U) ++service->correlated_serial;
            service->response_sync = 0U;
        }
    } else if (kind == ABOX_CLOUD_JOB_STATE && service->sync_pending) {
        if (service->correlated_serial == service->in_flight_correlated_serial)
            service->sync_pending = 0U;
        service->status_due = now + service->options.status_period_ms;
    } else if (kind == ABOX_CLOUD_JOB_STATE && service->event_pending) {
        if (service->event_serial == service->in_flight_event_serial)
            service->event_pending = 0U;
        service->status_due = now + service->options.status_period_ms;
    } else if (kind == ABOX_CLOUD_JOB_HEARTBEAT)
        service->heartbeat_due = now + service->options.heartbeat_period_ms;
    else if (kind == ABOX_CLOUD_JOB_STATE)
        service->status_due = now + service->options.status_period_ms;
    return kind;
}
