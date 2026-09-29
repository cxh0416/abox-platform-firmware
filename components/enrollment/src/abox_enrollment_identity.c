#include "abox_enrollment_identity.h"
#include <stdio.h>
#include <string.h>

int ABoxEnrollmentIdentity_Build(ABoxEnrollmentIdentity *identity,
                                  ABoxEnrollmentIdentityStorage *storage,
                                  uint32_t uid0, uint32_t uid1, uint32_t uid2,
                                  const char *iccid, const char *configured_vid,
                                  uint8_t vid_assigned,
                                  const char *fallback_prefix,
                                  const char *hardware_contract,
                                  const char *boot_version,
                                  const char *app_version)
{
    int n;
    if (!identity || !storage || !configured_vid || !fallback_prefix ||
        !hardware_contract || !*hardware_contract || !app_version ||
        !*app_version) return 0;
    n = snprintf(storage->uid, sizeof(storage->uid), "%08lX%08lX%08lX",
                 (unsigned long)uid0, (unsigned long)uid1, (unsigned long)uid2);
    if (n != 24) return 0;
    if (vid_assigned) {
        size_t length = strlen(configured_vid);
        if (!length || length >= sizeof(storage->vid)) return 0;
        memcpy(storage->vid, configured_vid, length + 1U);
    } else {
        n = snprintf(storage->vid, sizeof(storage->vid), "%s%s",
                     fallback_prefix, storage->uid);
        if (n <= 0 || (size_t)n >= sizeof(storage->vid)) return 0;
    }
    *identity = (ABoxEnrollmentIdentity){storage->uid, iccid ? iccid : "",
                                         storage->vid, hardware_contract,
                                         boot_version ? boot_version : "",
                                         app_version};
    return 1;
}
