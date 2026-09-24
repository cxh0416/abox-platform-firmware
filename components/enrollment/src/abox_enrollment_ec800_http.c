#include "abox_enrollment_ec800_http.h"

#include <stdio.h>
#include <string.h>

static void command_done(ABoxEc800Result result, void *user);

static int submit(ABoxEnrollmentEc800Http *h, const char *command, uint32_t timeout)
{
    return ABoxEc800At_Submit(h->at, command, h->owner, ABOX_EC800_PRIORITY_HIGH,
                             timeout, command_done, h);
}

static void finish(ABoxEnrollmentEc800Http *h, int clean)
{
    ABoxEnrollmentHttpDone done = h->done;
    void *user = h->user;
    uint16_t status = h->status;
    size_t length = h->response_length;
    if (!clean) h->blocked = 1U;
    h->state = ABOX_ENROLL_HTTP_IDLE;
    h->url = NULL; h->body = NULL; h->response = NULL;
    if (done) done(user, status, length, clean && !h->overflow && !h->cancelling);
}

static void stop_http(ABoxEnrollmentEc800Http *h)
{
    h->state = ABOX_ENROLL_HTTP_STOP;
    if (!submit(h, "AT+QHTTPSTOP", 10000U)) finish(h, 0);
}

static void command_done(ABoxEc800Result result, void *user)
{
    ABoxEnrollmentEc800Http *h = user;
    int count;
    if (!h || h->state == ABOX_ENROLL_HTTP_IDLE) return;
    if (h->state == ABOX_ENROLL_HTTP_STOP) {
        finish(h, result == ABOX_EC800_RESULT_OK);
        return;
    }
    if (result != ABOX_EC800_RESULT_OK || h->cancelling) {
        stop_http(h);
        return;
    }
    switch (h->state) {
    case ABOX_ENROLL_HTTP_TLS_VERSION:
        h->state = ABOX_ENROLL_HTTP_TLS_LEVEL;
        if (!submit(h, "AT+QSSLCFG=\"seclevel\",1,1", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_TLS_LEVEL:
        h->state = ABOX_ENROLL_HTTP_TLS_SNI;
        if (!submit(h, "AT+QSSLCFG=\"sni\",1,1", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_TLS_SNI:
        h->state = ABOX_ENROLL_HTTP_TLS_TIME;
        if (!submit(h, "AT+QSSLCFG=\"ignorelocaltime\",1,0", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_TLS_TIME:
        count = snprintf(h->command, sizeof(h->command),
                         "AT+QSSLCFG=\"cacert\",1,\"%s\"", h->ca_file);
        if (count < 0 || (size_t)count >= sizeof(h->command)) { stop_http(h); break; }
        h->state = ABOX_ENROLL_HTTP_TLS_CA;
        if (!submit(h, h->command, 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_TLS_CA:
        h->state = ABOX_ENROLL_HTTP_CONTEXT;
        if (!submit(h, "AT+QHTTPCFG=\"contextid\",1", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_CONTEXT:
        h->state = ABOX_ENROLL_HTTP_SSL_CONTEXT;
        if (!submit(h, "AT+QHTTPCFG=\"sslctxid\",1", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_SSL_CONTEXT:
        h->state = ABOX_ENROLL_HTTP_REQ_HEADER;
        if (!submit(h, "AT+QHTTPCFG=\"requestheader\",0", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_REQ_HEADER:
        h->state = ABOX_ENROLL_HTTP_RESP_HEADER;
        if (!submit(h, "AT+QHTTPCFG=\"responseheader\",0", 5000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_RESP_HEADER:
        count = snprintf(h->command, sizeof(h->command), "AT+QHTTPURL=%u,80",
                         (unsigned)strlen(h->url));
        if (count < 0 || (size_t)count >= sizeof(h->command)) { stop_http(h); break; }
        h->state = ABOX_ENROLL_HTTP_URL;
        if (!submit(h, h->command, 10000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_URL:
        if (!h->url_sent) { stop_http(h); break; }
        count = snprintf(h->command, sizeof(h->command), "AT+QHTTPPOST=%u,80,80",
                         (unsigned)strlen(h->body));
        if (count < 0 || (size_t)count >= sizeof(h->command)) { stop_http(h); break; }
        h->state = ABOX_ENROLL_HTTP_POST;
        if (!submit(h, h->command, 90000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_POST:
        if (!h->body_sent || !h->status || !h->expected_length ||
            h->expected_length >= h->response_capacity) { stop_http(h); break; }
        h->state = ABOX_ENROLL_HTTP_READ;
        if (!submit(h, "AT+QHTTPREAD=80", 90000U)) stop_http(h);
        break;
    case ABOX_ENROLL_HTTP_READ:
        if (h->response_length != h->expected_length) h->overflow = 1U;
        stop_http(h);
        break;
    default:
        stop_http(h);
        break;
    }
}

static void on_event(ABoxEc800Event event, const uint8_t *data,
                     uint16_t length, void *user)
{
    ABoxEnrollmentEc800Http *h = user;
    char line[96];
    unsigned long result, status, size;
    if (!h || h->state == ABOX_ENROLL_HTTP_IDLE || !data) return;
    if (event == ABOX_EC800_EVENT_RAW) {
        if (h->state != ABOX_ENROLL_HTTP_READ ||
            length > h->response_capacity - 1U - h->response_length) {
            h->overflow = 1U;
            return;
        }
        memcpy(h->response + h->response_length, data, length);
        h->response_length += length;
        h->response[h->response_length] = '\0';
        return;
    }
    if (length >= sizeof(line)) return;
    memcpy(line, data, length);
    line[length] = '\0';
    if (!strcmp(line, "CONNECT") && h->state == ABOX_ENROLL_HTTP_URL && !h->url_sent) {
        h->url_sent = ABoxEc800At_SendPayload(h->at, h->owner,
                                              (const uint8_t *)h->url, (uint16_t)strlen(h->url));
    } else if (!strcmp(line, "CONNECT") && h->state == ABOX_ENROLL_HTTP_POST && !h->body_sent) {
        h->body_sent = ABoxEc800At_SendPayload(h->at, h->owner,
                                               (const uint8_t *)h->body, (uint16_t)strlen(h->body));
    } else if (!strcmp(line, "CONNECT") && h->state == ABOX_ENROLL_HTTP_READ) {
        if (!ABoxEc800At_BeginRaw(h->at, h->owner, h->expected_length)) h->overflow = 1U;
    } else if (h->state == ABOX_ENROLL_HTTP_POST &&
               sscanf(line, "+QHTTPPOST: %lu,%lu,%lu", &result, &status, &size) == 3 &&
               result == 0UL && status <= 65535UL) {
        h->status = (uint16_t)status;
        h->expected_length = (uint32_t)size;
    }
}

int ABoxEnrollmentEc800Http_Init(ABoxEnrollmentEc800Http *h, ABoxEc800At *at,
                                 ABoxEc800Owner owner, const char *ca_file,
                                 ABoxEnrollmentHttpDone done, void *user)
{
    if (!h || !at || owner == ABOX_EC800_OWNER_NONE || !ca_file || !ca_file[0] ||
        strlen(ca_file) > 80U || !done) return 0;
    memset(h, 0, sizeof(*h));
    h->at = at; h->owner = owner; h->ca_file = ca_file; h->done = done; h->user = user;
    return ABoxEc800At_Register(at, owner, on_event, h);
}

int ABoxEnrollmentEc800Http_Ready(const ABoxEnrollmentEc800Http *h)
{ return h && !h->blocked && h->state == ABOX_ENROLL_HTTP_IDLE && !ABoxEc800At_IsBusy(h->at); }

int ABoxEnrollmentEc800Http_Post(ABoxEnrollmentEc800Http *h,
                                 const char *url, const char *body,
                                 char *response, size_t capacity)
{
    size_t url_length, body_length;
    if (!ABoxEnrollmentEc800Http_Ready(h) || !url || !body || !response || capacity < 2U)
        return 0;
    url_length = strlen(url); body_length = strlen(body);
    if (!url_length || url_length > UINT16_MAX || !body_length || body_length > UINT16_MAX ||
        strncmp(url, "https://", 8U)) return 0;
    h->url = url; h->body = body; h->response = response;
    h->response_capacity = capacity; h->response_length = 0U;
    h->expected_length = 0U; h->status = 0U;
    h->url_sent = 0U; h->body_sent = 0U;
    h->overflow = 0U; h->cancelling = 0U;
    response[0] = '\0';
    h->state = ABOX_ENROLL_HTTP_TLS_VERSION;
    if (!submit(h, "AT+QSSLCFG=\"sslversion\",1,3", 5000U)) {
        h->state = ABOX_ENROLL_HTTP_IDLE;
        return 0;
    }
    return 1;
}

void ABoxEnrollmentEc800Http_Cancel(ABoxEnrollmentEc800Http *h)
{ if (h && h->state != ABOX_ENROLL_HTTP_IDLE) h->cancelling = 1U; }

void ABoxEnrollmentEc800Http_AfterModemReset(ABoxEnrollmentEc800Http *h)
{
    if (!h) return;
    h->state = ABOX_ENROLL_HTTP_IDLE;
    h->blocked = 0U;
    h->cancelling = 0U;
    h->url = NULL; h->body = NULL; h->response = NULL;
}
