#ifndef ABOX_STM32F105_EC800_IDENTITY_H
#define ABOX_STM32F105_EC800_IDENTITY_H

#include "abox_enrollment_identity.h"

/* Hardware-only inputs (UID and frozen Boot descriptor) come from this port.
 * The product supplies its own identity, enrollment contract and ICCID policy. */
int ABoxStm32F105Ec800Identity_Build(ABoxEnrollmentIdentity *identity,
                                      ABoxEnrollmentIdentityStorage *storage,
                                      const char *iccid,
                                      const char *configured_vid,
                                      uint8_t vid_assigned,
                                      const char *fallback_prefix,
                                      const char *hardware_contract,
                                      const char *app_version);

#endif
