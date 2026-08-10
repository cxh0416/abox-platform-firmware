#ifndef ABOX_OTA_EC_AT_H
#define ABOX_OTA_EC_AT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void OTA_EC_AT_Init(void);
void OTA_EC_AT_Task(void);
void OTA_EC_AT_OnRx(uint8_t *data, uint16_t len);
void OTA_EC_SendAT(const char *cmd);
void OTA_EC_SendAT_F(const char *fmt, ...);
uint8_t OTA_EC_TestAT(void);
uint8_t OTA_EC_IsReady(void);
void OTA_EC_RequestCpin(void);
uint8_t OTA_EC_IsSimReady(void);
uint8_t OTA_EC_IsCpinDone(void);
void OTA_EC_RequestCsq(void);
uint8_t OTA_EC_IsCsqDone(void);
int OTA_EC_GetCsq(void);
void OTA_EC_RequestCereg(void);
void OTA_EC_RequestEchoOff(void);
uint8_t OTA_EC_IsCeregDone(void);
uint8_t OTA_EC_IsNetReady(void);
int OTA_EC_GetCeregStat(void);
uint8_t OTA_EC_HasCmeError(void);
int OTA_EC_GetCmeErrorCode(void);
void OTA_EC_ClearErrors(void);
void OTA_EC_RegisterLineHook(void (*hook)(const char *line));
void OTA_EC_RegisterRawHook(void (*hook)(const uint8_t *data, uint16_t len));
uint8_t OTA_EC_IsEchoOffDone(void);

#ifdef __cplusplus
}
#endif

#endif
