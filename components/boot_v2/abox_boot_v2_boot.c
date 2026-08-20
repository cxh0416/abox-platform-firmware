#include "abox_boot_v2_boot.h"
#include <stdio.h>
#include <string.h>

#define TRIAL_FAILURE_LIMIT 3U
#define RESET_REASON_IWDG 3U
static ABoxBootV2BootPort g_port;
static uint8_t g_bound;
static uint8_t g_attempted;

static void log_line(const char *event, const char *result)
{
    char line[128];
    if (!g_port.log) return;
    (void)snprintf(line, sizeof(line), "[BOOT2] evt=%s result=%s\r\n", event, result);
    g_port.log(g_port.context, line);
}
static int save(ABoxBootV2Record *record, const char *event){int ok=ABoxBootV2_StateSave(record);log_line(event,ok?"PASS":"FAIL");return ok;}
int ABoxBootV2Boot_Init(const ABoxBootV2BootPort *port)
{
    if(!port||!port->flash_image_valid||!port->install_slot||!port->jump_to_app||!port->reset_reason)return 0;
    g_port=*port;g_bound=1U;g_attempted=0U;return 1;
}
static void start_rollback(ABoxBootV2Record *r)
{
    if(r->stable_slot>1U||r->stable_image_size==0U){r->last_error=ABOX_BOOT_V2_ERROR_INSTALL_IO;(void)save(r,"ROLLBACK_NO_STABLE");return;}
    r->state=ABOX_BOOT_V2_ROLLBACK;r->rollback_installed=0U;(void)save(r,"ROLLBACK_START");
}
static void trial(ABoxBootV2Record *r)
{
    uint8_t bad=(uint8_t)(g_port.reset_reason(g_port.context)==RESET_REASON_IWDG||ABoxBootV2_FaultValid()||!g_port.flash_image_valid(g_port.context,r->image_size,r->image_crc32));
    r->last_reset_reason=g_port.reset_reason(g_port.context);
    if(bad){if(r->trial_fail_count<TRIAL_FAILURE_LIMIT)r->trial_fail_count++;r->last_error=ABOX_BOOT_V2_ERROR_TRIAL_FAULT;ABoxBootV2_FaultClear();if(r->trial_fail_count>=TRIAL_FAILURE_LIMIT)start_rollback(r);else(void)save(r,"TRIAL_FAILURE");}
    if(r->state==ABOX_BOOT_V2_TRIAL&&g_port.flash_image_valid(g_port.context,r->image_size,r->image_crc32))g_port.jump_to_app(g_port.context);
}
static void install(ABoxBootV2Record *r)
{
    ABoxBootV2InstallResult result;
    if (g_attempted) return;
    g_attempted = 1U;
    r->state = ABOX_BOOT_V2_INSTALLING;
    if (!save(r, "INSTALL_BEGIN")) {
        r->last_error = ABOX_BOOT_V2_ERROR_INSTALL_IO;
        if (r->stable_image_size != 0U && r->stable_slot <= 1U &&
            g_port.flash_image_valid(g_port.context, r->stable_image_size, r->stable_image_crc32)) {
            r->state = ABOX_BOOT_V2_CONFIRMED;
            r->candidate_slot = r->stable_slot;
            r->image_size = r->stable_image_size;
            r->image_crc32 = r->stable_image_crc32;
            memcpy(r->image_version, r->stable_image_version, sizeof(r->image_version));
            if (save(r, "INSTALL_STATE_FALLBACK")) g_port.jump_to_app(g_port.context);
        }
        return;
    }
    result=g_port.install_slot(g_port.context,r->candidate_slot,r->image_size,r->image_crc32);
    if(result==ABOX_BOOT_V2_INSTALL_OK){ABoxBootV2_FaultClear();r->state=ABOX_BOOT_V2_TRIAL;r->trial_fail_count=0U;if(save(r,"CANDIDATE_TRIAL"))g_port.jump_to_app(g_port.context);return;}
    r->last_error=(result==ABOX_BOOT_V2_INSTALL_PRE_ERASE_FAILED)?ABOX_BOOT_V2_ERROR_INSTALL_IO:ABOX_BOOT_V2_ERROR_INSTALL_CRC;
    if (result == ABOX_BOOT_V2_INSTALL_PRE_ERASE_FAILED && r->stable_image_size != 0U &&
        r->stable_slot <= 1U &&
        g_port.flash_image_valid(g_port.context, r->stable_image_size, r->stable_image_crc32)) {
        r->state = ABOX_BOOT_V2_CONFIRMED;
        r->candidate_slot = r->stable_slot;
        r->image_size = r->stable_image_size;
        r->image_crc32 = r->stable_image_crc32;
        memcpy(r->image_version, r->stable_image_version, sizeof(r->image_version));
        if (save(r, "INSTALL_PRE_ERASE_FALLBACK")) g_port.jump_to_app(g_port.context);
        return;
    }
    r->state = ABOX_BOOT_V2_ROLLBACK;
    r->rollback_installed = 0U;
    (void)save(r,"INSTALL_FAILED_ROLLBACK");
}
static void rollback(ABoxBootV2Record *r)
{
    if(r->rollback_installed&&g_port.flash_image_valid(g_port.context,r->stable_image_size,r->stable_image_crc32)){g_port.jump_to_app(g_port.context);return;}
    if(g_port.install_slot(g_port.context,r->stable_slot,r->stable_image_size,r->stable_image_crc32)==ABOX_BOOT_V2_INSTALL_OK){r->rollback_installed=1U;r->rollback_report_pending=1U;r->last_error=ABOX_BOOT_V2_ERROR_ROLLED_BACK;if(save(r,"ROLLBACK_DONE"))g_port.jump_to_app(g_port.context);return;}
    r->last_error=ABOX_BOOT_V2_ERROR_INSTALL_IO;(void)save(r,"ROLLBACK_FAILED");
}
void ABoxBootV2Boot_Task(void)
{
    ABoxBootV2Record r;
    if(!g_bound)return;
    if(!ABoxBootV2_StateLoad(&r)){if(g_port.app_vector_valid&&g_port.app_vector_valid(g_port.context))g_port.jump_to_app(g_port.context);return;}
    if(r.state==ABOX_BOOT_V2_TRIAL)trial(&r);
    else if(r.state==ABOX_BOOT_V2_INSTALL_PENDING||r.state==ABOX_BOOT_V2_INSTALLING)install(&r);
    else if(r.state==ABOX_BOOT_V2_ROLLBACK)rollback(&r);
    else if(g_port.flash_image_valid(g_port.context,r.image_size,r.image_crc32))g_port.jump_to_app(g_port.context);
    if(g_port.delay_ms)g_port.delay_ms(g_port.context,1000U);
}
