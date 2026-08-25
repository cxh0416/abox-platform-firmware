#ifndef ABOX_EC800_RX_H
#define ABOX_EC800_RX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABOX_EC800_RX_DMA_MIN 1088U
#define ABOX_EC800_RX_RING_MIN 1152U

typedef void (*ABoxEc800RxSink)(void *context, const uint8_t *data,
                                uint16_t length);

typedef struct {
    uint8_t *dma_buffer;
    uint16_t dma_size;
    uint8_t *ring_buffer;
    uint16_t ring_size;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t overflow_count;
    volatile uint32_t restart_count;
    volatile uint32_t max_backlog;
    volatile uint8_t restart_requested;
    uint16_t dma_position;
} ABoxEc800Rx;

int ABoxEc800Rx_Init(ABoxEc800Rx *rx, uint8_t *dma_buffer, uint16_t dma_size,
                     uint8_t *ring_buffer, uint16_t ring_size);
int ABoxEc800Rx_OnDmaEvent(ABoxEc800Rx *rx, uint16_t position);
uint32_t ABoxEc800Rx_Drain(ABoxEc800Rx *rx, ABoxEc800RxSink sink,
                           void *context, uint32_t limit);
int ABoxEc800Rx_TakeRestartRequest(ABoxEc800Rx *rx);
void ABoxEc800Rx_RequestRestart(ABoxEc800Rx *rx);
void ABoxEc800Rx_AfterRestart(ABoxEc800Rx *rx);
uint32_t ABoxEc800Rx_OverflowCount(const ABoxEc800Rx *rx);
uint32_t ABoxEc800Rx_RestartCount(const ABoxEc800Rx *rx);
uint32_t ABoxEc800Rx_MaxBacklog(const ABoxEc800Rx *rx);

#ifdef __cplusplus
}
#endif
#endif
