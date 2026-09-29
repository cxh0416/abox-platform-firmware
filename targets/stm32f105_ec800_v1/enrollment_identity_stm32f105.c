#include "abox_stm32f105_ec800_identity.h"

#include "abox_boot_v2.h"
#include "main.h"

int ABoxStm32F105Ec800Identity_Build(ABoxEnrollmentIdentity *identity,
                                      ABoxEnrollmentIdentityStorage *storage,
                                      const char *iccid,
                                      const char *configured_vid,
                                      uint8_t vid_assigned,
                                      const char *fallback_prefix,
                                      const char *hardware_contract,
                                      const char *app_version)
{
    static ABoxBootV2Descriptor descriptor;
    const char *boot_version = "";
    if (ABoxBootV2_DescriptorRead(&descriptor))
        boot_version = descriptor.build_version;
    return ABoxEnrollmentIdentity_Build(identity, storage,
                                        HAL_GetUIDw0(), HAL_GetUIDw1(),
                                        HAL_GetUIDw2(), iccid, configured_vid,
                                        vid_assigned, fallback_prefix,
                                        hardware_contract, boot_version,
                                        app_version);
}
