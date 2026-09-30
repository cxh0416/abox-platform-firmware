#ifndef ABOX_BOOT_V2_SERVICES_H
#define ABOX_BOOT_V2_SERVICES_H
#include "abox_boot_v2_ec800_port.h"
#include "abox_mqtt_ec800_bridge.h"
/* Bind the existing OTA machine to the same AT owner and MQTT workspace.
 * Call after bridge initialization. A caller-supplied transfer buffer remains
 * intact; otherwise the bridge RX payload is borrowed. Product version,
 * artifact, Flash layout, logs and safety admission stay with the caller. */
int ABoxBootV2Ec800Adapter_BindServices(ABoxBootV2Ec800Adapter *adapter,
    ABoxEc800At *at, ABoxMqttEc800Bridge *bridge, ABoxBootV2AppPort *port);
#endif
