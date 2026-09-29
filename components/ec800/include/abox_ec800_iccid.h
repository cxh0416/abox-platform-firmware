#ifndef ABOX_EC800_ICCID_H
#define ABOX_EC800_ICCID_H

#include "abox_ec800_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A single AT owner reads the SIM identity. Poll from the EC800 owner task. */
typedef struct {
    ABoxEc800At *at;
    ABoxEc800Owner owner;
    char value[32];
    uint32_t started_ms;
    uint8_t valid, pending;
} ABoxEc800Iccid;

int ABoxEc800Iccid_Init(ABoxEc800Iccid *identity, ABoxEc800At *at,
                         ABoxEc800Owner owner);
int ABoxEc800Iccid_Request(ABoxEc800Iccid *identity, uint32_t now_ms);
void ABoxEc800Iccid_Poll(ABoxEc800Iccid *identity, uint32_t now_ms);
void ABoxEc800Iccid_OnModemReset(ABoxEc800Iccid *identity);
const char *ABoxEc800Iccid_Value(const ABoxEc800Iccid *identity);

#ifdef __cplusplus
}
#endif
#endif
