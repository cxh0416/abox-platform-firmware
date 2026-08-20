#include "abox_boot_v2.h"
#include <string.h>
static ABoxBootV2Layout g_image_layout={0x0803E800U,0x0803F000U,0x800U,0x08008000U,0x0803E7FFU,0x20000000U,0x2000FFFFU};
int ABoxBootV2_ImageVectorValid(const uint8_t v[8]){uint32_t sp,reset;if(!v)return 0;memcpy(&sp,v,4U);memcpy(&reset,v+4U,4U);return sp>=g_image_layout.sram_start_addr&&sp<=g_image_layout.sram_end_addr&&(reset&1U)!=0U&&(reset&~1U)>=g_image_layout.app_start_addr&&(reset&~1U)<=g_image_layout.app_end_addr;}
