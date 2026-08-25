#include "abox_ec800_rx.h"

#include <string.h>

static uint32_t backlog(const ABoxEc800Rx *rx)
{
    return rx->head - rx->tail;
}

static int push_span(ABoxEc800Rx *rx, const uint8_t *data, uint16_t length)
{
    uint32_t used;
    uint32_t head;
    uint16_t first;

    if (length == 0U) return 1;
    used = backlog(rx);
    if (used > rx->ring_size || length > (uint32_t)rx->ring_size - used) {
        if (rx->overflow_count != 0xFFFFFFFFUL) ++rx->overflow_count;
        rx->restart_requested = 1U;
        return 0;
    }

    head = rx->head;
    first = (uint16_t)(rx->ring_size - (uint16_t)(head % rx->ring_size));
    if (first > length) first = length;
    memcpy(rx->ring_buffer + (head % rx->ring_size), data, first);
    if (length > first) memcpy(rx->ring_buffer, data + first, length - first);
    rx->head = head + length;
    used += length;
    if (used > rx->max_backlog) rx->max_backlog = used;
    return 1;
}

int ABoxEc800Rx_Init(ABoxEc800Rx *rx, uint8_t *dma_buffer, uint16_t dma_size,
                     uint8_t *ring_buffer, uint16_t ring_size)
{
    if (!rx || !dma_buffer || !ring_buffer || dma_size < ABOX_EC800_RX_DMA_MIN ||
        ring_size < ABOX_EC800_RX_RING_MIN)
        return 0;
    memset(rx, 0, sizeof(*rx));
    rx->dma_buffer = dma_buffer;
    rx->dma_size = dma_size;
    rx->ring_buffer = ring_buffer;
    rx->ring_size = ring_size;
    return 1;
}

int ABoxEc800Rx_OnDmaEvent(ABoxEc800Rx *rx, uint16_t position)
{
    uint16_t normalized;
    uint16_t previous;

    if (!rx || position > rx->dma_size || rx->restart_requested) return 0;
    previous = rx->dma_position;
    normalized = position == rx->dma_size ? 0U : position;

    if (position == rx->dma_size) {
        if (!push_span(rx, rx->dma_buffer + previous,
                       (uint16_t)(rx->dma_size - previous)))
            return 0;
    } else if (normalized >= previous) {
        if (!push_span(rx, rx->dma_buffer + previous,
                       (uint16_t)(normalized - previous)))
            return 0;
    } else {
        if (!push_span(rx, rx->dma_buffer + previous,
                       (uint16_t)(rx->dma_size - previous)) ||
            !push_span(rx, rx->dma_buffer, normalized))
            return 0;
    }
    rx->dma_position = normalized;
    return 1;
}

uint32_t ABoxEc800Rx_Drain(ABoxEc800Rx *rx, ABoxEc800RxSink sink,
                           void *context, uint32_t limit)
{
    uint32_t total = 0U;
    if (!rx || !sink || rx->restart_requested) return 0U;
    if (limit == 0U) limit = 0xFFFFFFFFUL;

    while (rx->tail != rx->head && total < limit) {
        uint32_t available = rx->head - rx->tail;
        uint32_t take = rx->ring_size - (rx->tail % rx->ring_size);
        if (take > available) take = available;
        if (take > limit - total) take = limit - total;
        sink(context, rx->ring_buffer + (rx->tail % rx->ring_size),
             (uint16_t)take);
        rx->tail += take;
        total += take;
    }
    return total;
}

int ABoxEc800Rx_TakeRestartRequest(ABoxEc800Rx *rx)
{
    if (!rx || !rx->restart_requested) return 0;
    rx->restart_requested = 0U;
    return 1;
}

void ABoxEc800Rx_RequestRestart(ABoxEc800Rx *rx)
{
    if (rx) rx->restart_requested = 1U;
}

void ABoxEc800Rx_AfterRestart(ABoxEc800Rx *rx)
{
    if (!rx) return;
    rx->head = 0U;
    rx->tail = 0U;
    rx->dma_position = 0U;
    rx->restart_requested = 0U;
    if (rx->restart_count != 0xFFFFFFFFUL) ++rx->restart_count;
}

uint32_t ABoxEc800Rx_OverflowCount(const ABoxEc800Rx *rx)
{
    return rx ? rx->overflow_count : 0U;
}

uint32_t ABoxEc800Rx_RestartCount(const ABoxEc800Rx *rx)
{
    return rx ? rx->restart_count : 0U;
}

uint32_t ABoxEc800Rx_MaxBacklog(const ABoxEc800Rx *rx)
{
    return rx ? rx->max_backlog : 0U;
}
