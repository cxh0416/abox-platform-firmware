#ifndef ABOX_ENROLLMENT_EC800_HTTP_H
#define ABOX_ENROLLMENT_EC800_HTTP_H

#include "abox_ec800_at.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ABoxEnrollmentHttpDone)(void *user, uint16_t status,
                                       size_t length, int cleanup_ok);

typedef enum {
    ABOX_ENROLL_HTTP_IDLE = 0,
    ABOX_ENROLL_HTTP_TLS_VERSION, ABOX_ENROLL_HTTP_TLS_LEVEL,
    ABOX_ENROLL_HTTP_TLS_SNI, ABOX_ENROLL_HTTP_TLS_TIME,
    ABOX_ENROLL_HTTP_TLS_CA, ABOX_ENROLL_HTTP_CONTEXT,
    ABOX_ENROLL_HTTP_SSL_CONTEXT, ABOX_ENROLL_HTTP_REQ_HEADER,
    ABOX_ENROLL_HTTP_RESP_HEADER, ABOX_ENROLL_HTTP_URL,
    ABOX_ENROLL_HTTP_POST, ABOX_ENROLL_HTTP_READ, ABOX_ENROLL_HTTP_STOP
} ABoxEnrollmentHttpState;

typedef struct {
    ABoxEc800At *at;
    ABoxEc800Owner owner;
    ABoxEnrollmentHttpDone done;
    void *user;
    const char *ca_file, *url, *body;
    char *response;
    size_t response_capacity, response_length;
    uint32_t expected_length;
    uint16_t status;
    uint8_t url_sent, body_sent, overflow, cancelling, blocked;
    ABoxEnrollmentHttpState state;
    char command[112];
} ABoxEnrollmentEc800Http;

int ABoxEnrollmentEc800Http_Init(ABoxEnrollmentEc800Http *http, ABoxEc800At *at,
                                 ABoxEc800Owner owner, const char *ca_file,
                                 ABoxEnrollmentHttpDone done, void *user);
int ABoxEnrollmentEc800Http_Post(ABoxEnrollmentEc800Http *http,
                                 const char *url, const char *body,
                                 char *response, size_t response_capacity);
void ABoxEnrollmentEc800Http_Cancel(ABoxEnrollmentEc800Http *http);
int ABoxEnrollmentEc800Http_Ready(const ABoxEnrollmentEc800Http *http);
/* Call only after the modem has physically reset and its AT core is reset. */
void ABoxEnrollmentEc800Http_AfterModemReset(ABoxEnrollmentEc800Http *http);

#ifdef __cplusplus
}
#endif
#endif
