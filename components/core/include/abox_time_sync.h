#ifndef ABOX_TIME_SYNC_H
#define ABOX_TIME_SYNC_H
#include <stdint.h>
typedef struct {
    uint64_t sent, next_due, best_sent, best_received, best_utc, seed;
    uint32_t sequence, best_error, best_rtt;
    uint8_t pending, attempts, ready, have_best;
    char request_id[64];
} ABoxTimeSync;
typedef struct {
    void *context;
    int (*send)(void *, const char *request_id);
    int (*apply)(void *, uint64_t server_utc, uint32_t error,
                uint64_t sent, uint64_t received);
} ABoxTimeSyncPort;
void ABoxTimeSync_Init(ABoxTimeSync *sync);
/* authenticated_ready requires server-authenticated transport, not just MQTT. */
void ABoxTimeSync_Poll(ABoxTimeSync *sync, const ABoxTimeSyncPort *port,
                        uint8_t authenticated_ready, uint64_t now, uint64_t boot_seed);
int ABoxTimeSync_Response(ABoxTimeSync *sync, const char *request_id,
                            uint64_t server_utc, uint32_t error, uint64_t received);
#endif
