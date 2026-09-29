#include <assert.h>
#include <string.h>

#include "abox_boot_v2.h"
#include "abox_stm32f105_ec800_identity.h"

static int descriptor_present;
uint32_t HAL_GetUIDw0(void) { return 0x05DAFF38U; }
uint32_t HAL_GetUIDw1(void) { return 0x37345252U; }
uint32_t HAL_GetUIDw2(void) { return 0x43054363U; }
int ABoxBootV2_DescriptorRead(ABoxBootV2Descriptor *descriptor)
{
    if (!descriptor_present) return 0;
    strcpy(descriptor->build_version, "abox-boot-2.3.0");
    return 1;
}

int main(void)
{
    ABoxEnrollmentIdentity identity;
    ABoxEnrollmentIdentityStorage storage;
    descriptor_present = 1;
    assert(ABoxStm32F105Ec800Identity_Build(&identity, &storage,
        "89860012345678901234", "UNASSIGNED", 0U, "SWP-",
        "sweeper_vcu_stm32f105_ec800_v1", "sweeper-vcu-1.3.7"));
    assert(strcmp(identity.uid, "05DAFF383734525243054363") == 0);
    assert(strcmp(identity.vid, "SWP-05DAFF383734525243054363") == 0);
    assert(strcmp(identity.iccid, "89860012345678901234") == 0);
    assert(strcmp(identity.boot_version, "abox-boot-2.3.0") == 0);
    descriptor_present = 0;
    assert(ABoxStm32F105Ec800Identity_Build(&identity, &storage,
        0, "BENCH-SW-054363", 1U, "SWP-",
        "sweeper_vcu_stm32f105_ec800_v1", "sweeper-vcu-1.3.7"));
    assert(strcmp(identity.vid, "BENCH-SW-054363") == 0);
    assert(strcmp(identity.boot_version, "") == 0);
    assert(!ABoxStm32F105Ec800Identity_Build(&identity, &storage,
        0, "UNASSIGNED", 0U, "PREFIX-TOO-LONG-",
        "sweeper_vcu_stm32f105_ec800_v1", "sweeper-vcu-1.3.7"));
    return 0;
}
