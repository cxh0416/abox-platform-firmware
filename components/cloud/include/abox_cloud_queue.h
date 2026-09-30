#ifndef ABOX_CLOUD_QUEUE_H
#define ABOX_CLOUD_QUEUE_H
#include <stddef.h>
#include <stdint.h>
/* Caller-owned fixed-size records. Keep Storage in Flash and Cursor in RAM;
 * no duplicate payload allocation. One owner reserves, fills and commits a
 * record synchronously. The active publish remains immutable outside FIFO. */
typedef struct { void *records; size_t stride; uint8_t capacity; } ABoxCloudQueueStorage;
typedef struct { uint8_t head, count; } ABoxCloudQueueCursor;
void ABoxCloudQueue_Reset(const ABoxCloudQueueStorage *storage, ABoxCloudQueueCursor *cursor);
void *ABoxCloudQueue_Reserve(const ABoxCloudQueueStorage *storage, const ABoxCloudQueueCursor *cursor);
int ABoxCloudQueue_Commit(const ABoxCloudQueueStorage *storage, ABoxCloudQueueCursor *cursor);
void *ABoxCloudQueue_Front(const ABoxCloudQueueStorage *storage, const ABoxCloudQueueCursor *cursor);
int ABoxCloudQueue_Pop(const ABoxCloudQueueStorage *storage, ABoxCloudQueueCursor *cursor);
#endif
