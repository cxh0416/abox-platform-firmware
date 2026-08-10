#ifndef ABOX_PLATFORM_PORT_H
#define ABOX_PLATFORM_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*ABox_GetTickMsFn)(void *context);
typedef int (*ABox_UartWriteFn)(void *context, const uint8_t *data, uint32_t length);
typedef int (*ABox_FlashBeginFn)(void *context);
typedef int (*ABox_FlashWriteFn)(void *context, uint32_t address, const uint8_t *data, uint32_t length);
typedef int (*ABox_FlashErasePageFn)(void *context, uint32_t address);
typedef void (*ABox_FlashEndFn)(void *context);
typedef void (*ABox_CriticalFn)(void *context);
typedef void (*ABox_LogWriteFn)(void *context, const char *line);
typedef int (*ABox_OtaIsReadingRawFn)(void *context);

typedef struct {
    uint32_t app_start_addr;
    uint32_t ota_info_addr;
    uint32_t flash_page_size;
} ABoxFlashLayout;

typedef struct {
    void *context;
    ABox_GetTickMsFn get_tick_ms;
    ABox_UartWriteFn uart_write;
    ABox_FlashBeginFn flash_begin;
    ABox_FlashWriteFn flash_write;
    ABox_FlashErasePageFn flash_erase_page;
    ABox_FlashEndFn flash_end;
    ABox_CriticalFn enter_critical;
    ABox_CriticalFn exit_critical;
    ABox_LogWriteFn log_write;
    ABox_OtaIsReadingRawFn ota_is_reading_raw;
    const ABoxFlashLayout *flash_layout;
} ABoxPlatformPort;

/* The port is deliberately scheduler-neutral; FreeRTOS and bare-metal adapt it. */
int ABox_PlatformPortIsValid(const ABoxPlatformPort *port);
int ABox_PlatformPortBind(const ABoxPlatformPort *port);
const ABoxPlatformPort *ABox_PlatformPortGet(void);
const ABoxFlashLayout *ABox_PlatformFlashLayoutGet(void);

uint32_t ABox_PortGetTickMs(void);
int ABox_PortUartWrite(const uint8_t *data, uint32_t length);
int ABox_PortFlashBegin(void);
int ABox_PortFlashWrite(uint32_t address, const uint8_t *data, uint32_t length);
int ABox_PortFlashErasePage(uint32_t address);
void ABox_PortFlashEnd(void);
void ABox_PortLog(const char *fmt, ...);
int ABox_PortOtaIsReadingRaw(void);

#ifdef __cplusplus
}
#endif

#endif
