#include <assert.h>
#include <string.h>

#include "abox_cloud_service.h"

static ABoxCloudJob next(ABoxCloudService *service, uint32_t now,
                         ABoxCloudJobKind kind)
{
    ABoxCloudJob job;
    assert(ABoxCloudService_Next(service, now, &job));
    assert(job.kind == kind);
    return job;
}

static void confirm(ABoxCloudService *service, ABoxCloudJob *job,
                    uint64_t operation, uint32_t now)
{
    assert(ABoxCloudService_Begin(service, job, operation, now));
    assert(ABoxCloudService_Receipt(service, job->generation, operation,
                                    1, now) == job->kind);
}

int main(void)
{
    ABoxCloudBootstrap compact;
    ABoxCloudService service;
    ABoxCloudServiceOptions options = {1000U, 3000U, 0U};
    ABoxCloudJob job;
    uint32_t generation;

    assert(!ABoxCloudBootstrap_Init(0));
    assert(ABoxCloudBootstrap_Init(&compact));
    assert(sizeof(compact) < 32U);
    assert(!ABoxCloudBootstrap_Next(&compact, &job));
    ABoxCloudBootstrap_SetReady(&compact, 1);
    assert(ABoxCloudBootstrap_Next(&compact, &job));
    assert(job.kind == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 1U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 2U, 1) == ABOX_CLOUD_JOB_NONE);
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 1U, 0) == ABOX_CLOUD_JOB_NONE);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 3U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 3U, 1) == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 4U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 4U, 1) == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "startup") == 0);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 5U));
    generation = job.generation;
    ABoxCloudBootstrap_NetworkChanged(&compact);
    assert(ABoxCloudBootstrap_Receipt(&compact, generation, 5U, 1) == ABOX_CLOUD_JOB_NONE);
    ABoxCloudBootstrap_SetReady(&compact, 1);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 6U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 6U, 1) == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 7U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 7U, 1) == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "startup") == 0);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 8U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 8U, 1) == ABOX_CLOUD_JOB_STATE);
    ABoxCloudBootstrap_SetReady(&compact, 0);
    ABoxCloudBootstrap_SetReady(&compact, 1);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 9U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 9U, 1) == ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 10U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 10U, 1) == ABOX_CLOUD_JOB_HEARTBEAT);
    assert(ABoxCloudBootstrap_Next(&compact, &job) && job.kind == ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "reconnect") == 0);
    assert(ABoxCloudBootstrap_Begin(&compact, &job, 11U));
    assert(ABoxCloudBootstrap_Receipt(&compact, job.generation, 11U, 1) == ABOX_CLOUD_JOB_STATE);
    assert(!ABoxCloudBootstrap_Active(&compact));

    assert(!ABoxCloudService_Init(&service, 0, 0U));
    assert(ABoxCloudService_Init(&service, &options, 100U));
    assert(!ABoxCloudService_Next(&service, 100U, &job));
    assert(ABoxCloudService_QueueResponse(&service, "req-1", 1));
    assert(!ABoxCloudService_QueueResponse(&service, "req-2", 0));
    ABoxCloudService_SetReady(&service, 1, 200U);
    job = next(&service, 200U, ABOX_CLOUD_JOB_MANIFEST);
    assert(ABoxCloudService_Begin(&service, &job, 10U, 200U));
    assert(!ABoxCloudService_Next(&service, 200U, &job));
    assert(ABoxCloudService_Receipt(&service, service.generation, 9U, 1, 201U)
           == ABOX_CLOUD_JOB_NONE);
    assert(ABoxCloudService_Receipt(&service, service.generation, 10U, 1, 201U)
           == ABOX_CLOUD_JOB_MANIFEST);
    job = next(&service, 201U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 11U, 201U);
    job = next(&service, 202U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "startup") == 0);
    confirm(&service, &job, 12U, 202U);

    job = next(&service, 203U, ABOX_CLOUD_JOB_RESPONSE);
    assert(strcmp(job.request_id, "req-1") == 0);
    confirm(&service, &job, 13U, 203U);
    job = next(&service, 204U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "sync_state") == 0);
    assert(strcmp(job.request_id, "req-1") == 0);
    assert(ABoxCloudService_Begin(&service, &job, 14U, 204U));
    generation = service.generation;
    ABoxCloudService_SetReady(&service, 0, 205U);
    assert(service.generation != generation);
    assert(ABoxCloudService_Receipt(&service, generation, 14U, 1, 206U)
           == ABOX_CLOUD_JOB_NONE);
    ABoxCloudService_SetReady(&service, 1, 207U);
    job = next(&service, 207U, ABOX_CLOUD_JOB_MANIFEST);
    confirm(&service, &job, 15U, 207U);
    job = next(&service, 207U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 16U, 207U);
    job = next(&service, 207U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "reconnect") == 0);
    confirm(&service, &job, 17U, 207U);
    job = next(&service, 208U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "sync_state") == 0);
    confirm(&service, &job, 18U, 208U);
    assert(!ABoxCloudService_Next(&service, 209U, &job));

    assert(ABoxCloudService_WakeState(&service, "power_change"));
    job = next(&service, 210U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "power_change") == 0);
    {
        ABoxCloudJob wrong = job;
        wrong.reason = "fabricated";
        assert(!ABoxCloudService_Begin(&service, &wrong, 19U, 210U));
    }
    assert(ABoxCloudService_Begin(&service, &job, 19U, 210U));
    assert(ABoxCloudService_Receipt(&service, job.generation, 19U, 0, 211U)
           == ABOX_CLOUD_JOB_NONE);
    job = next(&service, 212U, ABOX_CLOUD_JOB_STATE);
    confirm(&service, &job, 20U, 212U);
    assert(ABoxCloudService_WakeState(&service, "first"));
    job = next(&service, 213U, ABOX_CLOUD_JOB_STATE);
    assert(ABoxCloudService_Begin(&service, &job, 22U, 213U));
    assert(ABoxCloudService_WakeState(&service, "second"));
    assert(ABoxCloudService_Receipt(&service, job.generation, 22U, 1, 214U)
           == ABOX_CLOUD_JOB_STATE);
    job = next(&service, 214U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.reason, "second") == 0);
    confirm(&service, &job, 23U, 214U);
    assert(ABoxCloudService_QueueStateReport(&service, "report-1", "command_result"));
    job = next(&service, 215U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.request_id, "report-1") == 0);
    assert(strcmp(job.reason, "command_result") == 0);
    assert(ABoxCloudService_Begin(&service, &job, 24U, 215U));
    assert(ABoxCloudService_QueueStateReport(&service, "report-2", "sync_state"));
    assert(ABoxCloudService_Receipt(&service, job.generation, 24U, 1, 216U)
           == ABOX_CLOUD_JOB_STATE);
    job = next(&service, 216U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.request_id, "report-2") == 0);
    assert(strcmp(job.reason, "sync_state") == 0);
    confirm(&service, &job, 25U, 216U);
    job = next(&service, 1207U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 21U, 1207U);
    assert(!ABoxCloudService_Next(&service, 1208U, &job));
    options.status_period_ms = 0U;
    assert(ABoxCloudService_Init(&service, &options, 0U));
    ABoxCloudService_SetReady(&service, 1, 0U);
    job = next(&service, 0U, ABOX_CLOUD_JOB_MANIFEST);
    confirm(&service, &job, 31U, 0U);
    job = next(&service, 0U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 32U, 0U);
    job = next(&service, 0U, ABOX_CLOUD_JOB_STATE);
    confirm(&service, &job, 33U, 0U);
    assert(!ABoxCloudService_Next(&service, 999U, &job));
    job = next(&service, 1000U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 34U, 1000U);
    assert(!ABoxCloudService_Next(&service, 1500U, &job));

    options.response_preempts_bootstrap = 1U;
    assert(ABoxCloudService_Init(&service, &options, 0U));
    assert(ABoxCloudService_QueueStateReport(&service, "sync-1", "requested"));
    assert(ABoxCloudService_QueueResponse(&service, "sync-1", 0));
    ABoxCloudService_SetReady(&service, 1, 10U);
    job = next(&service, 10U, ABOX_CLOUD_JOB_RESPONSE);
    assert(strcmp(job.request_id, "sync-1") == 0);
    assert(ABoxCloudService_Begin(&service, &job, 100U, 10U));
    ABoxCloudService_CancelResponse(&service);
    assert(ABoxCloudService_Receipt(&service, job.generation, 100U, 1, 11U)
           == ABOX_CLOUD_JOB_NONE);
    assert(ABoxCloudService_QueueResponse(&service, "req-2", 0));
    job = next(&service, 12U, ABOX_CLOUD_JOB_RESPONSE);
    assert(strcmp(job.request_id, "req-2") == 0);
    confirm(&service, &job, 101U, 12U);
    job = next(&service, 12U, ABOX_CLOUD_JOB_MANIFEST);
    confirm(&service, &job, 102U, 12U);
    job = next(&service, 12U, ABOX_CLOUD_JOB_HEARTBEAT);
    confirm(&service, &job, 103U, 12U);
    job = next(&service, 12U, ABOX_CLOUD_JOB_STATE);
    confirm(&service, &job, 104U, 12U);
    job = next(&service, 12U, ABOX_CLOUD_JOB_STATE);
    assert(strcmp(job.request_id, "sync-1") == 0);
    confirm(&service, &job, 105U, 12U);
    return 0;
}
