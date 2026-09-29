#include "abox_log_ring.h"

#include <stdio.h>
#include <string.h>

static const char *level_name(uint8_t level)
{
    switch (level) {
        case 0U: return "E";
        case 1U: return "W";
        case 2U: return "I";
        default: return "D";
    }
}

static int dump_noise(const char *line)
{
    if (!line) return 1;
    return strstr(line, " AT TX owner=") != 0 ||
           strstr(line, " AT RX owner=") != 0 ||
           strstr(line, " AT PAY owner=") != 0 ||
           strstr(line, " MQTT heartbeat publish busy") != 0 ||
           strstr(line, " MQTT payload owner mismatch") != 0;
}

static uint16_t format_line(const ABoxLogRing *ring, uint16_t index,
                            char *output, uint16_t capacity,
                            ABoxLogFormatTime format_time)
{
    char timestamp[24];
    uint32_t tick;
    int written;
    if (!ring || !output || !capacity) return 0U;
    output[0] = '\0';
    index = (uint16_t)(index % ABOX_LOG_LINE_COUNT);
    tick = ring->ticks[index];
    if (!format_time || !format_time(tick, timestamp, sizeof(timestamp)))
        (void)snprintf(timestamp, sizeof(timestamp), "BOOT+%lu.%03lus",
                       (unsigned long)(tick / 1000U),
                       (unsigned long)(tick % 1000U));
    written = snprintf(output, capacity, "[%s] %s", timestamp,
                       ring->lines[index]);
    if (written < 0) return 0U;
    if (written >= capacity) return (uint16_t)(capacity - 1U);
    return (uint16_t)written;
}

void ABoxLogRing_Init(ABoxLogRing *ring)
{
    if (!ring) return;
    memset(ring, 0, sizeof(*ring));
    ring->next_sequence = 1U;
    ring->level = 2U;
}

void ABoxLogRing_Clear(ABoxLogRing *ring)
{
    uint8_t level;
    if (!ring) return;
    level = ring->level;
    memset(ring, 0, sizeof(*ring));
    ring->level = level;
    ring->next_sequence = 1U;
}

void ABoxLogRing_SetLevel(ABoxLogRing *ring, uint8_t level)
{
    if (ring) ring->level = level;
}

uint8_t ABoxLogRing_GetLevel(const ABoxLogRing *ring)
{
    return ring ? ring->level : 0U;
}

void ABoxLogRing_PushText(ABoxLogRing *ring, uint32_t tick,
                          uint8_t level, const char *tag, const char *message)
{
    uint16_t index;
    if (!ring || !message || level > ring->level) return;
    index = ring->write_index;
    ring->ticks[index] = tick;
    ring->levels[index] = level;
    ring->sequences[index] = ring->next_sequence++;
    (void)snprintf(ring->lines[index], ABOX_LOG_LINE_SIZE,
                   "%s %s %s\r\n", level_name(level), tag ? tag : "APP",
                   message);
    ring->write_index = (uint16_t)((index + 1U) % ABOX_LOG_LINE_COUNT);
    if (ring->count < ABOX_LOG_LINE_COUNT) ring->count++;
}

void ABoxLogRing_VPrintf(ABoxLogRing *ring, uint32_t tick,
                         uint8_t level, const char *tag,
                         const char *format, va_list arguments)
{
    char message[64];
    if (!ring || !format || level > ring->level ||
        vsnprintf(message, sizeof(message), format, arguments) < 0) return;
    ABoxLogRing_PushText(ring, tick, level, tag, message);
}

uint16_t ABoxLogRing_Dump(ABoxLogRing *ring, char *output,
                           uint16_t capacity, ABoxLogFormatTime format_time)
{
    uint16_t i, used = 0U, start, first, lengths[ABOX_LOG_LINE_COUNT];
    if (!ring || !output || !capacity) return 0U;
    output[0] = '\0';
    start = (ring->write_index + ABOX_LOG_LINE_COUNT - ring->count) %
            ABOX_LOG_LINE_COUNT;
    for (i = 0U; i < ring->count; ++i) {
        char line[128];
        lengths[i] = format_line(ring,
                                (uint16_t)((start + i) % ABOX_LOG_LINE_COUNT),
                                line, sizeof(line), format_time);
        if (dump_noise(line)) lengths[i] = 0U;
    }
    first = ring->count;
    for (i = ring->count; i > 0U; --i) {
        uint16_t length = lengths[(uint16_t)(i - 1U)];
        if (!length) continue;
        if (used + length + 1U >= capacity) break;
        used = (uint16_t)(used + length);
        first = (uint16_t)(i - 1U);
    }
    used = 0U;
    for (i = first; i < ring->count; ++i) {
        char line[128];
        uint16_t length = lengths[i];
        if (!length) continue;
        if (used + length + 1U >= capacity) break;
        (void)format_line(ring,
                          (uint16_t)((start + i) % ABOX_LOG_LINE_COUNT),
                          line, sizeof(line), format_time);
        memcpy(output + used, line, length);
        used = (uint16_t)(used + length);
        output[used] = '\0';
    }
    return used;
}

uint16_t ABoxLogRing_ReadMqttLine(ABoxLogRing *ring, char *output,
                                   uint16_t capacity,
                                   ABoxLogFormatTime format_time)
{
    uint16_t oldest, newest, length;
    if (!ring || !output || !capacity || !ring->count) return 0U;
    oldest = (ring->write_index + ABOX_LOG_LINE_COUNT - ring->count) %
             ABOX_LOG_LINE_COUNT;
    newest = ring->write_index;
    if (ring->count == ABOX_LOG_LINE_COUNT) {
        uint16_t distance = (ring->mqtt_read_index + ABOX_LOG_LINE_COUNT -
                             oldest) % ABOX_LOG_LINE_COUNT;
        if (distance >= ring->count) ring->mqtt_read_index = oldest;
    }
    if (ring->mqtt_read_index == newest) return 0U;
    do {
        length = format_line(ring, ring->mqtt_read_index, output,
                             capacity, format_time);
        ring->mqtt_read_index = (uint16_t)((ring->mqtt_read_index + 1U) %
                                            ABOX_LOG_LINE_COUNT);
        if (!dump_noise(output)) return length;
    } while (ring->mqtt_read_index != newest);
    output[0] = '\0';
    return 0U;
}

uint16_t ABoxLogRing_ReadMqttAlert(ABoxLogRing *ring, char *output,
                                    uint16_t capacity, uint8_t *level,
                                    ABoxLogFormatTime format_time)
{
    uint16_t i, best = ABOX_LOG_LINE_COUNT, length = 0U;
    uint32_t best_sequence = UINT32_MAX, oldest_sequence = UINT32_MAX;
    if (!ring || !output || !capacity) return 0U;
    output[0] = '\0';
    for (i = 0U; i < ABOX_LOG_LINE_COUNT; ++i) {
        uint32_t sequence = ring->sequences[i];
        if (sequence && sequence < oldest_sequence) oldest_sequence = sequence;
        if (sequence > ring->mqtt_sequence && sequence < best_sequence) {
            best_sequence = sequence;
            best = i;
        }
    }
    if (oldest_sequence != UINT32_MAX &&
        ring->mqtt_sequence + 1U < oldest_sequence)
        ring->mqtt_dropped += oldest_sequence - (ring->mqtt_sequence + 1U);
    while (best < ABOX_LOG_LINE_COUNT) {
        ring->mqtt_sequence = ring->sequences[best];
        if (ring->levels[best] <= 1U) {
            length = format_line(ring, best, output, capacity, format_time);
            if (level) *level = ring->levels[best];
            break;
        }
        best = ABOX_LOG_LINE_COUNT;
        best_sequence = UINT32_MAX;
        for (i = 0U; i < ABOX_LOG_LINE_COUNT; ++i) {
            uint32_t sequence = ring->sequences[i];
            if (sequence > ring->mqtt_sequence && sequence < best_sequence) {
                best_sequence = sequence;
                best = i;
            }
        }
    }
    return length;
}

uint16_t ABoxLogRing_ReadDebugLine(ABoxLogRing *ring, char *output,
                                    uint16_t capacity,
                                    ABoxLogFormatTime format_time)
{
    uint16_t oldest, newest, length = 0U;
    if (!ring || !output || !capacity) return 0U;
    if (!ring->count) return 0U;
    oldest = (ring->write_index + ABOX_LOG_LINE_COUNT - ring->count) %
             ABOX_LOG_LINE_COUNT;
    newest = ring->write_index;
    if (ring->count == ABOX_LOG_LINE_COUNT) {
        uint16_t distance = (ring->debug_read_index + ABOX_LOG_LINE_COUNT -
                             oldest) % ABOX_LOG_LINE_COUNT;
        if (distance >= ring->count) ring->debug_read_index = oldest;
    }
    if (ring->debug_read_index != newest) {
        length = format_line(ring, ring->debug_read_index, output,
                             capacity, format_time);
        ring->debug_read_index = (uint16_t)((ring->debug_read_index + 1U) %
                                             ABOX_LOG_LINE_COUNT);
    }
    return length;
}

uint32_t ABoxLogRing_MqttDropped(const ABoxLogRing *ring)
{
    return ring ? ring->mqtt_dropped : 0U;
}
