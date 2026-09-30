#include "abox_cloud_queue.h"
#include <string.h>
static int valid(const ABoxCloudQueueStorage *s, const ABoxCloudQueueCursor *c)
{ return s && c && s->records && s->stride && s->capacity && c->head < s->capacity && c->count <= s->capacity; }
void ABoxCloudQueue_Reset(const ABoxCloudQueueStorage *s, ABoxCloudQueueCursor *c)
{
    if (!s || !c || !s->records || !s->stride || !s->capacity) return;
    memset(s->records, 0, s->stride * s->capacity); c->head = c->count = 0;
}
void *ABoxCloudQueue_Reserve(const ABoxCloudQueueStorage *s, const ABoxCloudQueueCursor *c)
{
    if (!valid(s, c) || c->count == s->capacity) return 0;
    return (uint8_t *)s->records + ((c->head + c->count) % s->capacity) * s->stride;
}
int ABoxCloudQueue_Commit(const ABoxCloudQueueStorage *s, ABoxCloudQueueCursor *c)
{ if (!ABoxCloudQueue_Reserve(s, c)) return 0; ++c->count; return 1; }
void *ABoxCloudQueue_Front(const ABoxCloudQueueStorage *s, const ABoxCloudQueueCursor *c)
{ return valid(s, c) && c->count ? (uint8_t *)s->records + c->head * s->stride : 0; }
int ABoxCloudQueue_Pop(const ABoxCloudQueueStorage *s, ABoxCloudQueueCursor *c)
{
    void *record = ABoxCloudQueue_Front(s, c);
    if (!record) return 0;
    memset(record, 0, s->stride); c->head = (c->head + 1U) % s->capacity; --c->count;
    return 1;
}
