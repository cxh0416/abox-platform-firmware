#ifndef ABOX_BOOT_V2_EC800_PORT_H
#define ABOX_BOOT_V2_EC800_PORT_H

#include "abox_boot_v2_app.h"
#include "abox_ec800_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Static adapter; its lifetime must cover the ABoxBootV2App session. The
 * product retains ownership of the EC800 core and of MQTT/workspace policy. */
typedef struct {
    ABoxEc800At *at;
    ABoxBootV2AtDoneFn done;
    void *done_user;
    ABoxBootV2AtEventFn events;
    void *events_user;
} ABoxBootV2Ec800Adapter;

/* Installs only the common AT/RAW/URC/overflow functions and context.
 * Set tick, MQTT, log and transfer-buffer callbacks on port separately. */
int ABoxBootV2Ec800Adapter_Bind(ABoxBootV2Ec800Adapter *adapter,
                                ABoxEc800At *at, ABoxBootV2AppPort *port);

#ifdef __cplusplus
}
#endif

#endif
