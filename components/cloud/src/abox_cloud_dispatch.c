#include "abox_cloud_dispatch.h"
#include <string.h>
int ABoxCloudDispatch_Open(const ABoxCloudRoute *routes, size_t count,
    const char *topic, const char *payload, ABoxCloudRequestContext *request)
{
    size_t i;
    if (!request) return 0;
    memset(request, 0, sizeof(*request));
    if (!routes || !topic || !payload) return 0;
    for (i = 0; i < count; ++i) {
        if (routes[i].topic && routes[i].topic[0] && strcmp(routes[i].topic, topic) == 0) {
            request->topic = topic; request->payload = payload;
            request->length = strlen(payload); request->route = routes[i].route;
            return 1;
        }
    }
    return 0;
}
const ABoxMqttCommandDescriptor *ABoxCloudDispatch_FindCommand(
    const ABoxMqttV4Registry *registry, const char *name)
{
    size_t i;
    const ABoxMqttCommandDescriptor *command;
    if (!registry || !name) return 0;
    for (i = 0; i < registry->count; ++i) {
        command = ABoxMqttV4_FindCommand(registry->profiles[i], name);
        if (command) return command;
    }
    return 0;
}
