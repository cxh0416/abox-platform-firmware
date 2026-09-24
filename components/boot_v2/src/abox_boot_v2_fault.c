#include "abox_boot_v2.h"
static volatile ABoxBootV2FaultMailbox g_fault __attribute__((section(".noinit")));
static uint32_t crc(const ABoxBootV2FaultMailbox*f){return ABoxBootV2_Crc32((const void*)f,16U);}
void ABoxBootV2_FaultRecord(uint32_t reason,uint32_t pc,uint32_t lr){g_fault.magic=0x46544355U;g_fault.reason=reason;g_fault.pc=pc;g_fault.lr=lr;g_fault.crc32=crc((const ABoxBootV2FaultMailbox*)&g_fault);}
int ABoxBootV2_FaultValid(void){return g_fault.magic==0x46544355U&&g_fault.crc32==crc((const ABoxBootV2FaultMailbox*)&g_fault);}
void ABoxBootV2_FaultClear(void){g_fault.magic=0U;g_fault.crc32=0U;}
const volatile ABoxBootV2FaultMailbox*ABoxBootV2_FaultMailboxGet(void){return &g_fault;}
