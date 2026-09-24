#include "abox_enrollment.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

typedef struct {
    unsigned posts, trials, cancellations;
    int ready, accept_trial;
    char last_url[256], last_body[512];
    ABoxEnrollmentCredential last_credential;
} Mock;

static int ready(void *user) { return ((Mock *)user)->ready; }
static int identity(void *user, ABoxEnrollmentIdentity *id)
{
    (void)user;
    *id = (ABoxEnrollmentIdentity){"00112233445566778899AABB", "89860000000000000001", "PATROL-00112233445566778899AABB",
                                   "patrol_vehicle_chassis_stm32f105_ec800_v1", "boot-2.3", "patrol-0.3"};
    return 1;
}
static int post(void *user, const char *url, const char *body, char *response, size_t capacity)
{
    Mock *m = user;
    (void)response; (void)capacity;
    ++m->posts;
    strcpy(m->last_url, url); strcpy(m->last_body, body);
    return 1;
}
static void cancel_http(void *user) { ++((Mock *)user)->cancellations; }
static void cancel_trial(void *user) { ++((Mock *)user)->cancellations; }
static int start_trial(void *user, const ABoxEnrollmentCredential *credential)
{
    Mock *m = user;
    ++m->trials;
    m->last_credential = *credential;
    return m->accept_trial;
}
static void reply(ABoxEnrollment *e, char *buffer, uint16_t status, const char *json,
                  int cleanup_ok, uint32_t now)
{
    size_t n = strlen(json);
    memcpy(buffer, json, n);
    ABoxEnrollment_OnHttp(e, status, n, cleanup_ok, now);
}

int main(void)
{
    Mock m = {0};
    ABoxEnrollment e;
    char url[256], body[512], response[1024], request_id[48], token[64];
    ABoxEnrollmentBuffers buffers = {url, body, response, request_id, token,
                                     sizeof(url), sizeof(body), sizeof(response),
                                     sizeof(request_id), sizeof(token)};
    ABoxEnrollmentPort port = {&m, ready, identity, post, cancel_http, start_trial, cancel_trial};
    const char *approved = "{\"status\":\"approved\",\"vid\":\"CANON001\",\"mqtt\":{\"host\":\"mqtt.example\","
                           "\"port\":19683,\"username\":\"abox_device\",\"password\":\"secret-1\","
                           "\"tlsEnabled\":false,\"tlsProfileId\":\"\"}}";
    CHECK(ABoxEnrollment_Init(&e, &port, &buffers, "https://ota.example:20443"));
    ABoxEnrollment_Poll(&e, 0);
    CHECK(m.posts == 0);
    m.ready = 1; m.accept_trial = 1;
    ABoxEnrollment_Poll(&e, 0);
    CHECK(m.posts == 1 && strstr(m.last_body, "patrol_vehicle_chassis_stm32f105_ec800_v1"));
    reply(&e, response, 201, "{\"requestId\":\"request-1\",\"pollToken\":\"abc_DEF-123\",\"retryAfterSec\":5}", 1, 100);
    CHECK(e.have_request && !strcmp(request_id, "request-1"));
    ABoxEnrollment_Poll(&e, 5099);
    CHECK(m.posts == 1);
    ABoxEnrollment_Poll(&e, 5100);
    CHECK(m.posts == 2 && strstr(m.last_url, "/request-1/poll"));
    reply(&e, response, 202, "{\"status\":\"pending\"}", 1, 5101);
    ABoxEnrollment_Poll(&e, 125101);
    CHECK(m.posts == 3);
    reply(&e, response, 200, approved, 0, 125102); /* HTTP cleanup must succeed first. */
    CHECK(m.trials == 0 && e.have_request);
    ABoxEnrollment_Poll(&e, 245102);
    CHECK(m.posts == 4);
    reply(&e, response, 200, approved, 1, 245102);
    CHECK(e.state == ABOX_ENROLLMENT_TRIAL && m.trials == 1);
    CHECK(!strcmp(m.last_credential.vid, "CANON001"));
    ABoxEnrollment_TrialResult(&e, 0, 245103);
    CHECK(e.have_request && e.state == ABOX_ENROLLMENT_WAITING);
    ABoxEnrollment_Poll(&e, 365103);
    CHECK(m.posts == 5);
    reply(&e, response, 410, "{}", 1, 365104);
    CHECK(!e.have_request && !token[0]);
    ABoxEnrollment_Poll(&e, 485104);
    CHECK(m.posts == 6 && strstr(m.last_url, "/requests") && !strstr(m.last_url, "/poll"));
    reply(&e, response, 201, "{\"requestId\":\"request-2\",\"pollToken\":\"new_token\",\"retryAfterSec\":1}", 1, 485105);
    ABoxEnrollment_Poll(&e, 486105);
    reply(&e, response, 200, approved, 1, 486106);
    ABoxEnrollment_TrialResult(&e, 1, 486107);
    CHECK(e.state == ABOX_ENROLLMENT_COMPLETE && !e.have_request && !token[0] && !e.credential.password[0]);

    CHECK(ABoxEnrollment_Init(&e, &port, &buffers, "https://ota.example:20443"));
    ABoxEnrollment_Poll(&e, 0);
    reply(&e, response, 201, "{\"requestId\":\"request-3\",\"pollToken\":\"token-3\",\"retryAfterSec\":1}", 1, 1);
    ABoxEnrollment_Poll(&e, 1001);
    reply(&e, response, 200, "{\"status\":\"approved\",\"vid\":\"CANON001\",\"mqtt\":{\"host\":\"mqtt.example\","
            "\"port\":65536,\"username\":\"abox_device\",\"password\":\"secret-1\",\"tlsEnabled\":false}}", 1, 1002);
    CHECK(e.state == ABOX_ENROLLMENT_WAITING && m.trials == 2);
    ABoxEnrollment_Poll(&e, 121002);
    ABoxEnrollment_OnHttp(&e, 403, 0, 1, 121003);
    CHECK(!e.have_request && !token[0]);

    CHECK(ABoxEnrollment_Init(&e, &port, &buffers, "https://ota.example:20443"));
    ABoxEnrollment_Poll(&e, UINT32_MAX - 10U);
    ABoxEnrollment_Cancel(&e);
    CHECK(e.state == ABOX_ENROLLMENT_CANCELLING && m.cancellations == 1);
    ABoxEnrollment_OnHttp(&e, 0, 0, 1, 0);
    CHECK(e.cancelled && !e.have_request && e.state == ABOX_ENROLLMENT_WAITING);
    puts("ABox enrollment request, polling, cleanup, retry, trial and cancellation passed");
    return 0;
}
