#include "abox_enrollment_identity.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    ABoxEnrollmentIdentity identity;
    ABoxEnrollmentIdentityStorage storage;
    assert(ABoxEnrollmentIdentity_Build(&identity, &storage,
        0x05DAFF38U, 0x37345252U, 0x43054363U, 0, "UNASSIGNED", 0U,
        "SWP-", "sweeper_vcu_stm32f105_ec800_v1", "boot", "app"));
    assert(strcmp(identity.uid, "05DAFF383734525243054363") == 0);
    assert(strcmp(identity.vid, "SWP-05DAFF383734525243054363") == 0);
    assert(strcmp(identity.iccid, "") == 0);
    assert(ABoxEnrollmentIdentity_Build(&identity, &storage,
        1U, 2U, 3U, "89860012345678901234", "ASSIGNED-123", 1U,
        "MEAL-", "meal_delivery_stm32f105_ec800_v1", "boot", "app"));
    assert(strcmp(identity.vid, "ASSIGNED-123") == 0);
    assert(strcmp(identity.iccid, "89860012345678901234") == 0);
    assert(!ABoxEnrollmentIdentity_Build(&identity, &storage,
        1U, 2U, 3U, 0, "UNASSIGNED", 0U,
        "THIS-PREFIX-IS-TOO-LONG-", "contract", "boot", "app"));
    assert(!ABoxEnrollmentIdentity_Build(&identity, &storage,
        1U, 2U, 3U, 0, "", 1U, "MEAL-", "contract", "boot", "app"));
    return 0;
}
