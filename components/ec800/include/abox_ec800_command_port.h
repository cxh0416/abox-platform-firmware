#ifndef ABOX_EC800_COMMAND_PORT_H
#define ABOX_EC800_COMMAND_PORT_H
#include "abox_ec800_async.h"
#include "abox_ec800_at.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One outstanding operation; use a dedicated owner. Serialized with AT Task.
 * An active cancellation/timeout quarantines the entire AT stream, since an
 * untagged late OK cannot safely be assigned to any subsequent owner. */
typedef struct {
    ABoxEc800At *at;
    ABoxEc800Owner owner;
    uint64_t serial, operation;
    ABoxAsyncStatus result;
} ABoxEc800CommandAdapter;
int ABoxEc800CommandAdapter_Init(ABoxEc800CommandAdapter *adapter,
                                 ABoxEc800At *at, ABoxEc800Owner owner,
                                 ABoxEc800CommandPort *port);
/* Physical modem reset AND AT Reset must precede this call. */
void ABoxEc800CommandAdapter_OnModemReset(ABoxEc800CommandAdapter *adapter);
#ifdef __cplusplus
}
#endif
#endif
