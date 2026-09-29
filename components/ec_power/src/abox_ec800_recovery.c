#include "abox_ec800_recovery.h"

void ABoxEc800Recovery_Init(ABoxEc800Recovery *recovery, uint32_t cooldown_ms)
{
    if (!recovery) return;
    recovery->next_restart_ms = 0U;
    recovery->cooldown_ms = cooldown_ms;
    recovery->waiting = 0U;
}

int ABoxEc800Recovery_Poll(ABoxEc800Recovery *recovery, uint32_t now_ms,
                            int blocked, int can_restart)
{
    if (!recovery) return 0;
    if (!blocked) return 0;
    if (!can_restart) return 0;
    if (recovery->waiting && (int32_t)(now_ms - recovery->next_restart_ms) < 0)
        return 0;
    recovery->next_restart_ms = now_ms + recovery->cooldown_ms;
    recovery->waiting = 1U;
    return 1;
}
