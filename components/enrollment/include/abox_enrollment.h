#ifndef ABOX_ENROLLMENT_H
#define ABOX_ENROLLMENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *uid, *iccid, *vid, *hardware_contract, *boot_version, *app_version;
} ABoxEnrollmentIdentity;

typedef struct {
    char vid[32], host[64], username[32], password[64];
    uint16_t port;
    uint8_t tls_enabled, tls_profile_id;
} ABoxEnrollmentCredential;

typedef enum {
    ABOX_ENROLLMENT_WAITING = 0,
    ABOX_ENROLLMENT_HTTP,
    ABOX_ENROLLMENT_TRIAL,
    ABOX_ENROLLMENT_COMPLETE,
    ABOX_ENROLLMENT_CANCELLING
} ABoxEnrollmentState;

typedef struct {
    void *user;
    int (*ready)(void *user); /* Cellular, CA, HTTPS and OTA exclusion gate. */
    int (*identity)(void *user, ABoxEnrollmentIdentity *identity);
    /* POST owns the AT/TLS/HTTP lease until QHTTPSTOP cleanup. The response
     * buffer belongs to the caller; call OnHttp only after cleanup succeeds. */
    int (*post)(void *user, const char *url, const char *body,
                char *response, size_t capacity);
    void (*cancel_http)(void *user);
    int (*start_trial)(void *user, const ABoxEnrollmentCredential *credential);
    void (*cancel_trial)(void *user);
} ABoxEnrollmentPort;

typedef struct {
    char *url, *body, *response, *request_id, *poll_token;
    size_t url_size, body_size, response_size, request_id_size, poll_token_size;
} ABoxEnrollmentBuffers;

typedef struct {
    ABoxEnrollmentPort port;
    ABoxEnrollmentBuffers buffers;
    const char *origin;
    ABoxEnrollmentCredential credential;
    uint32_t retry_started, retry_delay;
    uint8_t have_request, waiting_trial, due, cancelled;
    ABoxEnrollmentState state;
} ABoxEnrollment;

int ABoxEnrollment_Init(ABoxEnrollment *enrollment, const ABoxEnrollmentPort *port,
                        const ABoxEnrollmentBuffers *buffers, const char *https_origin);
void ABoxEnrollment_Poll(ABoxEnrollment *enrollment, uint32_t now);
void ABoxEnrollment_OnHttp(ABoxEnrollment *enrollment, uint16_t status,
                           size_t response_length, int cleanup_ok, uint32_t now);
void ABoxEnrollment_TrialResult(ABoxEnrollment *enrollment, int saved_and_read_back,
                                uint32_t now);
void ABoxEnrollment_Cancel(ABoxEnrollment *enrollment);
ABoxEnrollmentState ABoxEnrollment_State(const ABoxEnrollment *enrollment);

#ifdef __cplusplus
}
#endif
#endif
