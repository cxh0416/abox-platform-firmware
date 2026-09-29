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
    ABoxCloudService service;
    ABoxCloudServiceOptions options = {1000U, 3000U};
    ABoxCloudJob job;
    uint32_t generation;

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
    return 0;
}
