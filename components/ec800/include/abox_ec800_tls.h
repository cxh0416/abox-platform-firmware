#ifndef ABOX_EC800_TLS_H
#define ABOX_EC800_TLS_H
#include "abox_ec800_ufs.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Software table capacity, NOT a claim about any EC800 firmware capability. */
#define ABOX_TLS_CONTEXT_CAPACITY 8U
#define ABOX_TLS_LEGACY_HTTPS_CONTEXT 1U
typedef struct { uint8_t context; uint64_t generation; } ABoxTlsLease;
#define ABOX_TLS_PSK_IDENTITY_SIZE 32U
#define ABOX_TLS_PSK_SECRET_SIZE 33U
typedef enum { ABOX_TLS_AUTH_CA = 0, ABOX_TLS_AUTH_PSK = 1 } ABoxTlsAuthMode;
/* Secret is exactly 32 ASCII bytes, never a hex-decoded key. Provision locally
 * with a CSPRNG. Neither this structure nor its AT command may be logged. */
typedef struct {
    uint32_t generation;
    char identity[ABOX_TLS_PSK_IDENTITY_SIZE];
    char secret[ABOX_TLS_PSK_SECRET_SIZE];
    uint8_t mode;
} ABoxTlsCredentials;
int ABoxTlsCredentials_Valid(const ABoxTlsCredentials *credentials);
typedef struct {
    const char *ca_file;
    uint32_t ca_revision;
    /* Caller attests trusted provenance AND exact UFS readback verification.
     * CA filenames are immutable/versioned while any lease uses them. */
    uint8_t ca_verified;
    uint8_t time_valid;
    ABoxTlsCredentials credentials;
} ABoxTlsProfile;
typedef struct {
    uint64_t generation, operation;
    uint32_t owner, revision, started, timeout;
    uint16_t pins;
    uint8_t step, waiting;
    /* Only one authentication mode is prepared per lease. */
    union {
        char ca_file[ABOX_EC800_UFS_PATH_SIZE];
        ABoxTlsCredentials credentials;
    };
    uint8_t auth_mode;
    ABoxAsyncStatus status;
} ABoxTlsSlot;
typedef struct {
    ABoxEc800CommandPort port;
    ABoxTlsSlot slots[ABOX_TLS_CONTEXT_CAPACITY];
    uint64_t serial;
    uint8_t supported_mask;
} ABoxEc800Tls;
/* One manager per physical modem. Init only once, before use. Context 1 is
 * always reserved for unmodified legacy HTTPS OTA, even after modem reset. */
int ABoxEc800Tls_Init(ABoxEc800Tls *tls, const ABoxEc800CommandPort *port,
                      uint8_t verified_supported_mask);
/* Update only after probing the physical modem and while no leases exist. */
int ABoxEc800Tls_SetSupportedMask(ABoxEc800Tls *tls,
                                  uint8_t verified_supported_mask);
int ABoxEc800Tls_Acquire(ABoxEc800Tls *tls, uint8_t context, uint32_t owner,
                         ABoxTlsLease *lease);
int ABoxEc800Tls_Release(ABoxEc800Tls *tls, ABoxTlsLease lease);
int ABoxEc800Tls_Prepare(ABoxEc800Tls *tls, ABoxTlsLease lease,
                         const ABoxTlsProfile *profile, uint32_t now, uint32_t timeout);
void ABoxEc800Tls_Poll(ABoxEc800Tls *tls, uint32_t now);
ABoxAsyncStatus ABoxEc800Tls_Status(const ABoxEc800Tls *tls, ABoxTlsLease lease);
int ABoxEc800Tls_Pin(ABoxEc800Tls *tls, ABoxTlsLease lease);
int ABoxEc800Tls_Unpin(ABoxEc800Tls *tls, ABoxTlsLease lease);
void ABoxEc800Tls_Cancel(ABoxEc800Tls *tls, ABoxTlsLease lease);
/* Physical reset + transport drain must precede this; all leases become stale. */
void ABoxEc800Tls_OnModemReset(ABoxEc800Tls *tls);
#ifdef __cplusplus
}
#endif
#endif
