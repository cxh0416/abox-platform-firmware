#ifndef ABOX_ENROLLMENT_IDENTITY_H
#define ABOX_ENROLLMENT_IDENTITY_H

#include "abox_enrollment.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char uid[25];
    char vid[32];
} ABoxEnrollmentIdentityStorage;

/* Product identity and hardware contract are explicit inputs. The platform
 * formats the STM32 UID and fallback VID without inventing product policy. */
int ABoxEnrollmentIdentity_Build(ABoxEnrollmentIdentity *identity,
                                  ABoxEnrollmentIdentityStorage *storage,
                                  uint32_t uid0, uint32_t uid1, uint32_t uid2,
                                  const char *iccid, const char *configured_vid,
                                  uint8_t vid_assigned,
                                  const char *fallback_prefix,
                                  const char *hardware_contract,
                                  const char *boot_version,
                                  const char *app_version);

#ifdef __cplusplus
}
#endif
#endif
