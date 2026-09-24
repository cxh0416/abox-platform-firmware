#include "abox_enrollment.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#define ENROLL_RETRY_MS 120000U

static int text_copy(char *dst, size_t capacity, const char *src)
{
    size_t length;
    if (!dst || !capacity || !src) return 0;
    length = strlen(src);
    if (length >= capacity) return 0;
    memcpy(dst, src, length + 1U);
    return 1;
}

static int safe_text(const char *value, size_t max_length, const char *extra)
{
    size_t i;
    if (!value) return 0;
    for (i = 0; value[i]; ++i) {
        char ch = value[i];
        if (i >= max_length || !((ch >= 'A' && ch <= 'Z') ||
                                 (ch >= 'a' && ch <= 'z') ||
                                 (ch >= '0' && ch <= '9') ||
                                 (extra && strchr(extra, ch)))) return 0;
    }
    return 1;
}

static void schedule(ABoxEnrollment *e, uint32_t now, uint32_t delay)
{
    e->retry_started = now;
    e->retry_delay = delay;
    e->due = 0U;
    e->state = ABOX_ENROLLMENT_WAITING;
}

static void forget_request(ABoxEnrollment *e)
{
    e->have_request = 0U;
    e->buffers.request_id[0] = '\0';
    e->buffers.poll_token[0] = '\0';
}

int ABoxEnrollment_Init(ABoxEnrollment *e, const ABoxEnrollmentPort *port,
                        const ABoxEnrollmentBuffers *buffers, const char *origin)
{
    size_t length;
    if (!e || !port || !port->ready || !port->identity || !port->post ||
        !port->start_trial || !port->cancel_http || !port->cancel_trial ||
        !buffers || !buffers->url || !buffers->body || !buffers->response ||
        !buffers->request_id || !buffers->poll_token ||
        buffers->url_size < 80U || buffers->body_size < 160U ||
        buffers->response_size < 256U || buffers->request_id_size < 37U ||
        buffers->poll_token_size < 44U || !origin) return 0;
    length = strlen(origin);
    if (length < 9U || length > 160U || strncmp(origin, "https://", 8U) ||
        origin[length - 1U] == '/') return 0;
    memset(e, 0, sizeof(*e));
    e->port = *port;
    e->buffers = *buffers;
    e->origin = origin;
    e->due = 1U;
    buffers->request_id[0] = '\0';
    buffers->poll_token[0] = '\0';
    return 1;
}

void ABoxEnrollment_Poll(ABoxEnrollment *e, uint32_t now)
{
    ABoxEnrollmentIdentity id = {0};
    int count;
    if (!e || e->cancelled || e->state != ABOX_ENROLLMENT_WAITING) return;
    if (!e->due && (uint32_t)(now - e->retry_started) < e->retry_delay) return;
    if (!e->port.ready(e->port.user)) return;
    if (e->have_request) {
        count = snprintf(e->buffers.url, e->buffers.url_size,
                         "%s/api/v1/enroll/abox/requests/%s/poll",
                         e->origin, e->buffers.request_id);
        if (count < 0 || (size_t)count >= e->buffers.url_size) return;
        count = snprintf(e->buffers.body, e->buffers.body_size,
                         "{\"pollToken\":\"%s\"}", e->buffers.poll_token);
    } else {
        if (!e->port.identity(e->port.user, &id) ||
            !safe_text(id.uid, 24U, "") || strlen(id.uid) != 24U ||
            !safe_text(id.iccid, 32U, "") ||
            !safe_text(id.vid, 31U, "_-") || !id.vid[0] ||
            !safe_text(id.hardware_contract, 80U, "_") || !id.hardware_contract[0] ||
            !safe_text(id.boot_version, 64U, "_.-") ||
            !safe_text(id.app_version, 64U, "_.-")) return;
        count = snprintf(e->buffers.url, e->buffers.url_size,
                         "%s/api/v1/enroll/abox/requests", e->origin);
        if (count < 0 || (size_t)count >= e->buffers.url_size) return;
        count = snprintf(e->buffers.body, e->buffers.body_size,
                         "{\"uid\":\"%s\",\"iccid\":\"%s\",\"vid\":\"%s\","
                         "\"hardwareContract\":\"%s\",\"bootVersion\":\"%s\","
                         "\"appVersion\":\"%s\"}", id.uid, id.iccid, id.vid,
                         id.hardware_contract, id.boot_version, id.app_version);
    }
    if (count < 0 || (size_t)count >= e->buffers.body_size) return;
    if (e->port.post(e->port.user, e->buffers.url, e->buffers.body,
                     e->buffers.response, e->buffers.response_size))
        e->state = ABOX_ENROLLMENT_HTTP;
    else schedule(e, now, ENROLL_RETRY_MS);
}

static uint32_t retry_after(const cJSON *root)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "retryAfterSec");
    double value;
    if (!cJSON_IsNumber(item)) return ENROLL_RETRY_MS;
    value = item->valuedouble;
    if (value < 1.0 || value > 2147483.0 || (double)(uint32_t)value != value)
        return ENROLL_RETRY_MS;
    return (uint32_t)value * 1000U;
}

static int credential_parse(ABoxEnrollmentCredential *out, const cJSON *root)
{
    const cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
    const cJSON *vid = cJSON_GetObjectItemCaseSensitive(root, "vid");
    const cJSON *host = cJSON_GetObjectItemCaseSensitive(mqtt, "host");
    const cJSON *port = cJSON_GetObjectItemCaseSensitive(mqtt, "port");
    const cJSON *user = cJSON_GetObjectItemCaseSensitive(mqtt, "username");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(mqtt, "password");
    const cJSON *tls = cJSON_GetObjectItemCaseSensitive(mqtt, "tlsEnabled");
    const cJSON *profile = cJSON_GetObjectItemCaseSensitive(mqtt, "tlsProfileId");
    if (!cJSON_IsString(vid) || !cJSON_IsString(host) || !cJSON_IsNumber(port) ||
        !cJSON_IsString(user) || !cJSON_IsString(password) ||
        !(cJSON_IsTrue(tls) || cJSON_IsFalse(tls)) ||
        port->valuedouble < 1.0 || port->valuedouble > 65535.0 ||
        (double)(uint16_t)port->valuedouble != port->valuedouble ||
        !safe_text(vid->valuestring, 31U, "_-") || !vid->valuestring[0] ||
        !safe_text(host->valuestring, 63U, ".-:") || !host->valuestring[0] ||
        !safe_text(user->valuestring, 31U, "_-") || !user->valuestring[0] ||
        !safe_text(password->valuestring, 63U, "_-@.!~") || !password->valuestring[0]) return 0;
    if (cJSON_IsTrue(tls) && (!cJSON_IsString(profile) || strcmp(profile->valuestring, "1")))
        return 0;
    if (!text_copy(out->vid, sizeof(out->vid), vid->valuestring) ||
        !text_copy(out->host, sizeof(out->host), host->valuestring) ||
        !text_copy(out->username, sizeof(out->username), user->valuestring) ||
        !text_copy(out->password, sizeof(out->password), password->valuestring)) return 0;
    out->port = (uint16_t)port->valuedouble;
    out->tls_enabled = cJSON_IsTrue(tls) ? 1U : 0U;
    out->tls_profile_id = out->tls_enabled ? 1U : 0U;
    return 1;
}

void ABoxEnrollment_OnHttp(ABoxEnrollment *e, uint16_t status,
                           size_t length, int cleanup_ok, uint32_t now)
{
    cJSON *root;
    const cJSON *item;
    uint32_t delay = ENROLL_RETRY_MS;
    if (!e || (e->state != ABOX_ENROLLMENT_HTTP &&
               e->state != ABOX_ENROLLMENT_CANCELLING)) return;
    if (e->state == ABOX_ENROLLMENT_CANCELLING) {
        memset(&e->credential, 0, sizeof(e->credential));
        forget_request(e);
        e->state = ABOX_ENROLLMENT_WAITING;
        return;
    }
    if (!cleanup_ok) {
        schedule(e, now, ENROLL_RETRY_MS);
        return;
    }
    if (e->have_request && (status == 401U || status == 403U || status == 404U || status == 410U)) {
        forget_request(e);
        schedule(e, now, ENROLL_RETRY_MS);
        return;
    }
    if (!length || length >= e->buffers.response_size) {
        schedule(e, now, ENROLL_RETRY_MS);
        return;
    }
    e->buffers.response[length] = '\0';
    if (status == 409U || status >= 500U || status == 0U) {
        schedule(e, now, ENROLL_RETRY_MS);
        return;
    }
    root = cJSON_ParseWithLength(e->buffers.response, length);
    if (!root) { schedule(e, now, ENROLL_RETRY_MS); return; }
    if (!e->have_request && (status == 200U || status == 201U)) {
        const cJSON *request_id = cJSON_GetObjectItemCaseSensitive(root, "requestId");
        const cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "pollToken");
        if (cJSON_IsString(request_id) && cJSON_IsString(token) &&
            safe_text(request_id->valuestring, 47U, "-") &&
            safe_text(token->valuestring, 63U, "_-") &&
            text_copy(e->buffers.request_id, e->buffers.request_id_size, request_id->valuestring) &&
            text_copy(e->buffers.poll_token, e->buffers.poll_token_size, token->valuestring)) {
            e->have_request = 1U;
            delay = retry_after(root);
        }
    } else if (e->have_request && status == 202U) {
        delay = retry_after(root);
    } else if (e->have_request && status == 200U) {
        item = cJSON_GetObjectItemCaseSensitive(root, "status");
        if (cJSON_IsString(item) && !strcmp(item->valuestring, "approved") &&
            credential_parse(&e->credential, root) &&
            e->port.start_trial(e->port.user, &e->credential)) {
            e->state = ABOX_ENROLLMENT_TRIAL;
            e->waiting_trial = 1U;
            cJSON_Delete(root);
            return;
        }
        memset(&e->credential, 0, sizeof(e->credential));
    }
    cJSON_Delete(root);
    schedule(e, now, delay);
}

void ABoxEnrollment_TrialResult(ABoxEnrollment *e, int saved, uint32_t now)
{
    if (!e || !e->waiting_trial) return;
    e->waiting_trial = 0U;
    memset(&e->credential, 0, sizeof(e->credential));
    if (e->cancelled) {
        forget_request(e);
        e->state = ABOX_ENROLLMENT_WAITING;
    } else if (saved) {
        forget_request(e);
        e->state = ABOX_ENROLLMENT_COMPLETE;
    } else schedule(e, now, ENROLL_RETRY_MS);
}

void ABoxEnrollment_Cancel(ABoxEnrollment *e)
{
    if (!e || e->state == ABOX_ENROLLMENT_COMPLETE) return;
    e->cancelled = 1U;
    if (e->state == ABOX_ENROLLMENT_HTTP) {
        e->state = ABOX_ENROLLMENT_CANCELLING;
        e->port.cancel_http(e->port.user);
    } else if (e->state == ABOX_ENROLLMENT_TRIAL) {
        e->state = ABOX_ENROLLMENT_CANCELLING;
        e->port.cancel_trial(e->port.user);
    } else {
        forget_request(e);
        memset(&e->credential, 0, sizeof(e->credential));
    }
}

ABoxEnrollmentState ABoxEnrollment_State(const ABoxEnrollment *e)
{ return e ? e->state : ABOX_ENROLLMENT_WAITING; }
