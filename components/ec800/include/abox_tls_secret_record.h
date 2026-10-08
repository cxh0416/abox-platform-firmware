#ifndef ABOX_TLS_SECRET_RECORD_H
#define ABOX_TLS_SECRET_RECORD_H
#include "abox_ec800_tls.h"
/* Optional append-only Flash extension. Product config V2/V5 bytes stay frozen.
 * Checksum detects corruption; it is not encryption or tamper protection. */
typedef struct {
    uint32_t magic, checksum;
    ABoxTlsCredentials credentials;
} ABoxTlsSecretRecord;
void ABoxTlsSecretRecord_Encode(ABoxTlsSecretRecord *, const ABoxTlsCredentials *);
/* Erased/legacy extension -> CA. Malformed extension -> invalid mode, fail closed. */
int ABoxTlsSecretRecord_Decode(const ABoxTlsSecretRecord *, ABoxTlsCredentials *);
#endif
