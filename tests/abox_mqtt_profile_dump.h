#ifndef ABOX_MQTT_PROFILE_DUMP_H
#define ABOX_MQTT_PROFILE_DUMP_H

#include <stdio.h>
#include "abox_mqtt_v4.h"

/* Host-only description of the production descriptor, consumed by the
 * generic JSON-to-C registry check. Descriptor text is protocol metadata. */
static void ABoxMqttProfile_Dump(const ABoxMqttProfileDescriptor *profile)
{
    size_t i;
    printf("profile\t%s\t%s\n", profile->name, profile->version);
    for (i = 0; i < profile->command_count; ++i)
        printf("command\t%s\t%d\n", profile->commands[i].name,
               (int)profile->commands[i].classification);
    for (i = 0; i < profile->report_count; ++i)
        printf("report\t%s\t%s\n", profile->reports[i].name,
               profile->reports[i].report_type);
}

#endif
