#ifndef ABOX_BOOT_OTA_IMAGE_H
#define ABOX_BOOT_OTA_IMAGE_H
#include <stdint.h>
#include "boot_cfg.h"
static inline uint32_t ABoxBoot_ReadLe32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);}
static inline uint8_t ABoxBoot_VectorValid(const uint8_t *p){uint32_t sp,reset;if(!p)return 0U;sp=ABoxBoot_ReadLe32(p);reset=ABoxBoot_ReadLe32(p+4U);return(sp>=0x20000000U&&sp<=0x20010000U&&(reset&1U)&&reset>=APP_START_ADDR&&reset<=APP_END_ADDR)?1U:0U;}
#endif
