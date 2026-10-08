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
    ABOX_ENROLL_HTTP_TLS_ITEMS, ABOX_ENROLL_HTTP_TLS_SIGNATURE,
    ABOX_ENROLL_HTTP_TLS_CHAIN, ABOX_ENROLL_HTTP_TLS_CIPHER,
    ABOX_ENROLL_HTTP_TLS_CA, ABOX_ENROLL_HTTP_CONTEXT,
    ABOX_ENROLL_HTTP_SSL_CONTEXT, ABOX_ENROLL_HTTP_REQ_HEADER,
    ABOX_ENROLL_HTTP_RESP_HEADER, ABOX_ENROLL_HTTP_URL,
    ABOX_ENROLL_HTTP_POST, ABOX_ENROLL_HTTP_READ, ABOX_ENROLL_HTTP_STOP,
    ABOX_ENROLL_HTTP_PDP_QUERY, ABOX_ENROLL_HTTP_PDP_ACTIVATE,
    ABOX_ENROLL_CA_LIST, ABOX_ENROLL_CA_OPEN, ABOX_ENROLL_CA_READ,
    ABOX_ENROLL_CA_CLOSE, ABOX_ENROLL_CA_DELETE, ABOX_ENROLL_CA_UPLOAD
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
    uint8_t url_sent, body_sent, overflow, cancelling, blocked, pdp_active;
    ABoxEnrollmentHttpState state;
    char command[112];
    const uint8_t *ca_pem;
    uint32_t ca_length, ca_offset, ca_handle;
    uint8_t ca_verified, ca_match, ca_handle_valid, ca_uploaded;
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
int ABoxEnrollmentEc800Http_SetCaPem(ABoxEnrollmentEc800Http *http,
                                    const uint8_t *pem, uint32_t length);
/* Poll only in the existing enrollment/OTA exclusion window. Exact UFS
 * length and byte readback are mandatory before trusting this CA. */
void ABoxEnrollmentEc800Http_PrepareCa(ABoxEnrollmentEc800Http *http);

#ifdef __cplusplus
}
#endif
#endif
