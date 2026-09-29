#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "abox_log_ring.h"

static int fixed_time(uint32_t tick, char *output, uint16_t capacity)
{
    (void)tick;
    if (capacity < 5U) return 0;
    strcpy(output, "TIME");
    return 1;
}

static void write_log(ABoxLogRing *ring, uint32_t tick,
                      uint8_t level, const char *tag, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    ABoxLogRing_VPrintf(ring, tick, level, tag, format, arguments);
    va_end(arguments);
}

int main(void)
{
    ABoxLogRing ring;
    char output[512];
    uint8_t level = 0xFFU;
    unsigned i;
    ABoxLogRing_Init(&ring);
    assert(ABoxLogRing_GetLevel(&ring) == 2U);
    write_log(&ring, 1234U, 2U, "APP", "ready %u", 3U);
    write_log(&ring, 1235U, 3U, "APP", "hidden");
    assert(ABoxLogRing_Dump(&ring, output, sizeof(output), 0) > 0U);
    assert(strcmp(output, "[BOOT+1.234s] I APP ready 3\r\n") == 0);
    assert(ABoxLogRing_ReadMqttLine(&ring, output, sizeof(output), fixed_time) > 0U);
    assert(strcmp(output, "[TIME] I APP ready 3\r\n") == 0);
    assert(ABoxLogRing_ReadMqttAlert(&ring, output, sizeof(output), &level,
                                      fixed_time) == 0U);
    ABoxLogRing_SetLevel(&ring, 3U);
    write_log(&ring, 2000U, 1U, "EC", "AT TX owner=busy");
    write_log(&ring, 2001U, 0U, "CLOUD", "failed");
    assert(ABoxLogRing_ReadMqttAlert(&ring, output, sizeof(output), &level,
                                      fixed_time) > 0U);
    assert(level == 1U && strstr(output, "AT TX owner=busy") != 0);
    assert(ABoxLogRing_ReadMqttAlert(&ring, output, sizeof(output), &level,
                                      fixed_time) > 0U);
    assert(level == 0U && strstr(output, "E CLOUD failed") != 0);
    assert(ABoxLogRing_ReadMqttLine(&ring, output, sizeof(output), fixed_time) > 0U);
    assert(strstr(output, "E CLOUD failed") != 0);
    assert(ABoxLogRing_ReadDebugLine(&ring, output, sizeof(output), fixed_time) > 0U);
    ABoxLogRing_Clear(&ring);
    assert(ABoxLogRing_GetLevel(&ring) == 3U);
    assert(ABoxLogRing_MqttDropped(&ring) == 0U);
    for (i = 0U; i < ABOX_LOG_LINE_COUNT + 3U; ++i)
        write_log(&ring, i, 1U, "APP", "entry %u", i);
    assert(ABoxLogRing_ReadMqttAlert(&ring, output, sizeof(output), &level,
                                      fixed_time) > 0U);
    assert(ABoxLogRing_MqttDropped(&ring) == 3U);
    assert(strstr(output, "entry 3") != 0);
    return 0;
}
