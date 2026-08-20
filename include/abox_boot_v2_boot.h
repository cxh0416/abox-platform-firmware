#ifndef ABOX_BOOT_V2_BOOT_H
#define ABOX_BOOT_V2_BOOT_H
#include <stdint.h>
#include "abox_boot_v2.h"
typedef enum { ABOX_BOOT_V2_INSTALL_OK=0, ABOX_BOOT_V2_INSTALL_PRE_ERASE_FAILED, ABOX_BOOT_V2_INSTALL_FAILED } ABoxBootV2InstallResult;
typedef struct {
    void *context;
    uint8_t (*flash_image_valid)(void *context, uint32_t size, uint32_t crc32);
    ABoxBootV2InstallResult (*install_slot)(void *context, uint8_t slot, uint32_t size, uint32_t crc32);
    uint8_t (*app_vector_valid)(void *context);
    void (*jump_to_app)(void *context);
    uint8_t (*reset_reason)(void *context);
    void (*delay_ms)(void *context, uint32_t delay_ms);
    void (*log)(void *context, const char *message);
} ABoxBootV2BootPort;
int ABoxBootV2Boot_Init(const ABoxBootV2BootPort *port);
void ABoxBootV2Boot_Task(void);
#endif
