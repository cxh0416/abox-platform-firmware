#ifndef ABOX_PLATFORM_PORT_H
#define ABOX_PLATFORM_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*ABox_GetTickMsFn)(void *context);
typedef int (*ABox_UartWriteFn)(void *context, const uint8_t *data, uint32_t length);
typedef int (*ABox_FlashWriteFn)(void *context, uint32_t address, const uint8_t *data, uint32_t length);
typedef int (*ABox_FlashErasePageFn)(void *context, uint32_t address);
typedef void (*ABox_CriticalFn)(void *context);

typedef struct {
    void *context;
    ABox_GetTickMsFn get_tick_ms;
    ABox_UartWriteFn uart_write;
    ABox_FlashWriteFn flash_write;
    ABox_FlashErasePageFn flash_erase_page;
    ABox_CriticalFn enter_critical;
    ABox_CriticalFn exit_critical;
} ABoxPlatformPort;

/* The port is deliberately scheduler-neutral; FreeRTOS and bare-metal adapt it. */
int ABox_PlatformPortIsValid(const ABoxPlatformPort *port);

#ifdef __cplusplus
}
#endif

#endif
