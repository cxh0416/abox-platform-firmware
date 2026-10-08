#ifndef ABOX_PSK_STATION_H
#define ABOX_PSK_STATION_H
#include "abox_ec800_tls.h"
/* Optional SWD station mailbox. OFF in normal firmware. No network endpoint. */
typedef struct {
    ABoxTlsCredentials credentials;
    char host[64], username[32], password[64];
    uint16_t port, reserved;
} ABoxPskStationInput;
_Static_assert(sizeof(ABoxPskStationInput) == 236U, "station wire layout changed");
extern volatile uint32_t ABoxPskStation_Request, ABoxPskStation_Completed, ABoxPskStation_Result;
extern ABoxPskStationInput *ABoxPskStation_Input;
/* Product supplies existing idle factory scratch; no extra payload allocation. */
void ABoxPskStation_SetInputBuffer(void *storage, size_t capacity);
/* callback must COPY candidate into the existing trial, never retain input. */
void ABoxPskStation_Poll(int (*stage)(const ABoxPskStationInput *));
#endif
