#ifndef ABOX_EC_POWER_H
#define ABOX_EC_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t off_hold_ms;
    uint32_t rail_stable_ms;
    uint32_t key_hold_ms;
    uint32_t restart_off_ms;
    uint8_t init_force_off;
} ABoxEcPowerConfig;

/* Configure the product-specific timing profile before EC_Power_Init(). */
uint8_t ABox_EcPower_SetConfig(const ABoxEcPowerConfig *config);

void EC_Power_Init(void);
void EC_Power_StartOnSequence(void);
void EC_Power_StartRestartSequence(void);
void EC_Power_Task(void);
void EC_Power_ForceOff(void);
uint8_t EC_Power_IsBusy(void);
uint8_t EC_Power_IsOnDone(void);
uint8_t EC_Power_IsDone(void);

#ifdef __cplusplus
}
#endif

#endif
