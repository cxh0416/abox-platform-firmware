#ifndef ABOX_CLOUD_DISPATCH_H
#define ABOX_CLOUD_DISPATCH_H
#include <stddef.h>
#include "abox_mqtt_v4.h"
typedef struct {
    const char *topic;
    const char *payload;
    size_t length;
    unsigned route;
} ABoxCloudRequestContext;
typedef struct {
    const char *topic;
    unsigned route;
} ABoxCloudRoute;
/* Topic buffers may change on identity switches; registration stores their
 * stable addresses. Products preserve their own public/internal parsers and
 * persisted business idempotency. Dispatch allocates no payload or JSON tree. */
int ABoxCloudDispatch_Open(const ABoxCloudRoute *routes, size_t count,
    const char *topic, const char *payload, ABoxCloudRequestContext *request);
const ABoxMqttCommandDescriptor *ABoxCloudDispatch_FindCommand(
    const ABoxMqttV4Registry *registry, const char *name);
#endif
