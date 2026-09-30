#include "abox_cloud_dispatch.h"
#include "abox_cloud_queue.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    unsigned records[2] = {0, 0};
    ABoxCloudQueueStorage storage = {records, sizeof(records[0]), 2};
    ABoxCloudQueueCursor cursor;
    unsigned *slot;
    char topic[32] = "/device/first/request";
    ABoxCloudRoute routes[] = {{topic, 7}};
    ABoxCloudRequestContext request;
    const ABoxMqttCommandDescriptor commands[] = {{"command", "p", ABOX_MQTT_COMMAND_QUERY}};
    const ABoxMqttProfileDescriptor profile = {"p", "1", commands, 1, 0, 0};
    const ABoxMqttProfileDescriptor *profiles[] = {&profile};
    const ABoxMqttV4Registry registry = {profiles, 1};
    ABoxCloudQueue_Reset(&storage, &cursor);
    slot = ABoxCloudQueue_Reserve(&storage, &cursor); assert(slot); *slot = 11;
    assert(ABoxCloudQueue_Commit(&storage, &cursor));
    slot = ABoxCloudQueue_Reserve(&storage, &cursor); assert(slot); *slot = 22;
    assert(ABoxCloudQueue_Commit(&storage, &cursor));
    assert(!ABoxCloudQueue_Reserve(&storage, &cursor));
    assert(*(unsigned *)ABoxCloudQueue_Front(&storage, &cursor) == 11);
    assert(ABoxCloudQueue_Pop(&storage, &cursor));
    slot = ABoxCloudQueue_Reserve(&storage, &cursor); assert(slot); *slot = 33;
    assert(ABoxCloudQueue_Commit(&storage, &cursor));
    assert(*(unsigned *)ABoxCloudQueue_Front(&storage, &cursor) == 22);
    assert(ABoxCloudQueue_Pop(&storage, &cursor));
    assert(*(unsigned *)ABoxCloudQueue_Front(&storage, &cursor) == 33);
    ABoxCloudQueue_Reset(&storage, &cursor);
    assert(!ABoxCloudQueue_Front(&storage, &cursor));
    assert(!ABoxCloudQueue_Pop(&storage, &cursor));
    assert(ABoxCloudDispatch_Open(routes, 1, topic, "{}", &request));
    assert(request.route == 7 && request.length == 2);
    strcpy(topic, "/device/second/request");
    assert(!ABoxCloudDispatch_Open(routes, 1, "/device/first/request", "{}", &request));
    assert(ABoxCloudDispatch_Open(routes, 1, topic, "{}", &request));
    assert(ABoxCloudDispatch_FindCommand(&registry, "command") == commands);
    assert(!ABoxCloudDispatch_FindCommand(&registry, "other"));
    return 0;
}
