#include "abox_config_store.h"

ABoxConfigStoreResult ABoxConfigStore_Commit(const ABoxConfigStorePort *port,
                                             const void *candidate,
                                             const void *stable)
{
    if (!port || !port->write_and_readback || !candidate || !stable ||
        candidate == stable)
        return ABOX_CONFIG_STORE_RECOVERY_FAILED;
    if (port->write_and_readback(port->context, candidate))
        return ABOX_CONFIG_STORE_SAVED;
    if (port->write_and_readback(port->context, stable))
        return ABOX_CONFIG_STORE_RECOVERED;
    return ABOX_CONFIG_STORE_RECOVERY_FAILED;
}
