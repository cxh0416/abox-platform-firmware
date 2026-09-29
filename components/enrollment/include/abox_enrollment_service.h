#ifndef ABOX_ENROLLMENT_SERVICE_H
#define ABOX_ENROLLMENT_SERVICE_H

#include "abox_enrollment.h"
#include "abox_enrollment_ec800_http.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Product policy and storage stay outside the service. trial_result returns
 * -1 while pending (including restore failure), 0 on failure, 1 after commit
 * and persistent readback. All callbacks run in the EC800 owner context. */
typedef struct {
    void *user;
    uint32_t (*now_ms)(void *);
    int (*network_ready)(void *);
    int (*needs_enrollment)(void *);
    int (*identity)(void *, ABoxEnrollmentIdentity *);
    void (*mqtt_pause)(void *, uint8_t);
    int (*start_trial)(void *, const ABoxEnrollmentCredential *);
    int (*trial_result)(void *);
    void (*cancel_trial)(void *);
} ABoxEnrollmentServicePort;

typedef struct {
    ABoxEnrollment enrollment;
    ABoxEnrollmentEc800Http http;
    ABoxEnrollmentServicePort port;
    uint8_t initialized;
} ABoxEnrollmentService;

int ABoxEnrollmentService_Init(ABoxEnrollmentService *service, ABoxEc800At *at,
                               ABoxEc800Owner owner, const char *ca_file,
                               const char *origin,
                               const ABoxEnrollmentBuffers *buffers,
                               uint8_t tls_profile_id,
                               const ABoxEnrollmentServicePort *port);
void ABoxEnrollmentService_Poll(ABoxEnrollmentService *service);
ABoxEnrollmentState ABoxEnrollmentService_State(const ABoxEnrollmentService *service);
void ABoxEnrollmentService_Cancel(ABoxEnrollmentService *service);
void ABoxEnrollmentService_AfterModemReset(ABoxEnrollmentService *service);

#ifdef __cplusplus
}
#endif
#endif
