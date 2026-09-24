#ifndef ABOX_OTA_FLASH_H
#define ABOX_OTA_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t OTA_Flash_Begin(void);
uint8_t OTA_Flash_Write(const uint8_t *data, uint16_t len);
void OTA_Flash_End(void);
void OTA_Flash_Abort(void);
uint32_t OTA_Flash_GetWrittenSize(void);

#ifdef __cplusplus
}
#endif

#endif
