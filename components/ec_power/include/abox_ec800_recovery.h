#ifndef ABOX_EC800_RECOVERY_H
#define ABOX_EC800_RECOVERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A quarantined AT session can only be reused after a physical modem reset.
 * The service owner decides whether OTA and product maintenance permit it,
 * then starts the EC800 rail sequence when Poll returns true. */
typedef struct {
    uint32_t next_restart_ms;
    uint32_t cooldown_ms;
    uint8_t waiting;
} ABoxEc800Recovery;

void ABoxEc800Recovery_Init(ABoxEc800Recovery *recovery, uint32_t cooldown_ms);
int ABoxEc800Recovery_Poll(ABoxEc800Recovery *recovery, uint32_t now_ms,
                            int blocked, int can_restart);

#ifdef __cplusplus
}
#endif

#endif
