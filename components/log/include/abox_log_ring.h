#ifndef ABOX_LOG_RING_H
#define ABOX_LOG_RING_H

#include <stdarg.h>
#include <stdint.h>

#define ABOX_LOG_LINE_COUNT 32U
#define ABOX_LOG_LINE_SIZE 80U

/* Product supplies the clock formatter and guards calls shared with an ISR.
 * No UART, scheduler or product name is owned by this ring. */
typedef int (*ABoxLogFormatTime)(uint32_t tick, char *output,
                                 uint16_t capacity);

typedef struct {
    char lines[ABOX_LOG_LINE_COUNT][ABOX_LOG_LINE_SIZE];
    uint32_t ticks[ABOX_LOG_LINE_COUNT];
    uint32_t sequences[ABOX_LOG_LINE_COUNT];
    uint8_t levels[ABOX_LOG_LINE_COUNT];
    uint16_t write_index;
    uint16_t count;
    uint16_t mqtt_read_index;
    uint16_t debug_read_index;
    uint32_t next_sequence;
    uint32_t mqtt_sequence;
    uint32_t mqtt_dropped;
    uint8_t level;
} ABoxLogRing;

void ABoxLogRing_Init(ABoxLogRing *ring);
void ABoxLogRing_Clear(ABoxLogRing *ring);
void ABoxLogRing_SetLevel(ABoxLogRing *ring, uint8_t level);
uint8_t ABoxLogRing_GetLevel(const ABoxLogRing *ring);
void ABoxLogRing_PushText(ABoxLogRing *ring, uint32_t tick,
                          uint8_t level, const char *tag, const char *message);
void ABoxLogRing_VPrintf(ABoxLogRing *ring, uint32_t tick,
                         uint8_t level, const char *tag,
                         const char *format, va_list arguments);
uint16_t ABoxLogRing_Dump(ABoxLogRing *ring, char *output,
                           uint16_t capacity, ABoxLogFormatTime format_time);
uint16_t ABoxLogRing_ReadMqttLine(ABoxLogRing *ring, char *output,
                                   uint16_t capacity, ABoxLogFormatTime format_time);
uint16_t ABoxLogRing_ReadMqttAlert(ABoxLogRing *ring, char *output,
                                    uint16_t capacity, uint8_t *level,
                                    ABoxLogFormatTime format_time);
uint16_t ABoxLogRing_ReadDebugLine(ABoxLogRing *ring, char *output,
                                    uint16_t capacity, ABoxLogFormatTime format_time);
uint32_t ABoxLogRing_MqttDropped(const ABoxLogRing *ring);

#endif
