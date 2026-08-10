#include "ota_ec_at.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "abox_platform_port.h"

#define OTA_EC_LINE_BUF_SIZE 256U

typedef enum
{
    OTA_EC_ST_IDLE = 0,
    OTA_EC_ST_WAIT_AT_RSP,
    OTA_EC_ST_READY
} OtaEcState_t;

typedef struct
{
    OtaEcState_t state;
    uint32_t state_ts;
    char line_buf[OTA_EC_LINE_BUF_SIZE];
    uint16_t line_len;
    uint8_t got_ok;
    uint8_t got_error;
    uint8_t echo_off_done;
    uint8_t cme_error;
    int cme_error_code;
    uint8_t cpin_done;
    uint8_t sim_ready;
    uint8_t csq_done;
    int csq_rssi;
    uint8_t cereg_done;
    int cereg_n;
    int cereg_stat;
    uint8_t net_ready;
} OtaEcCtx_t;

static OtaEcCtx_t g_ota_ec;
static void (*g_line_hook)(const char *line);
static void (*g_raw_hook)(const uint8_t *data, uint16_t len);

static void OTA_EC_OnLine(const char *line)
{
    if ((line == 0) || (line[0] == '\0')) return;

    if (strcmp(line, "OK") == 0)
    {
        g_ota_ec.got_ok = 1U;
        g_ota_ec.echo_off_done = 1U;
    }
    else if (strcmp(line, "ERROR") == 0) g_ota_ec.got_error = 1U;
    else if (strncmp(line, "+CME ERROR:", 11) == 0)
    {
        int code = -1;
        g_ota_ec.got_error = 1U;
        g_ota_ec.cme_error = 1U;
        g_ota_ec.cme_error_code = (sscanf(line, "+CME ERROR: %d", &code) == 1) ? code : -1;
        ABox_PortLog("[EC] cme=%d\r\n", g_ota_ec.cme_error_code);
    }
    else if (strncmp(line, "+CPIN:", 6) == 0)
    {
        g_ota_ec.cpin_done = 1U;
        g_ota_ec.sim_ready = (strstr(line, "READY") != 0) ? 1U : 0U;
    }
    else if (strncmp(line, "+CSQ:", 5) == 0)
    {
        int rssi = -1;
        int ber = -1;
        g_ota_ec.csq_done = 1U;
        g_ota_ec.csq_rssi = (sscanf(line, "+CSQ: %d,%d", &rssi, &ber) == 2) ? rssi : -1;
    }
    else if (strncmp(line, "+CEREG:", 7) == 0)
    {
        int n = -1;
        int stat = -1;
        int count = sscanf(line, "+CEREG: %d,%d", &n, &stat);
        if (count != 2)
        {
            count = sscanf(line, "+CEREG: %d", &stat);
            n = -1;
        }
        g_ota_ec.cereg_done = 1U;
        g_ota_ec.cereg_n = (count == 1 || count == 2) ? n : -1;
        g_ota_ec.cereg_stat = (count == 1 || count == 2) ? stat : -1;
        g_ota_ec.net_ready = ((g_ota_ec.cereg_stat == 1) || (g_ota_ec.cereg_stat == 5)) ? 1U : 0U;
    }

    if (g_line_hook) g_line_hook(line);
}

void OTA_EC_AT_Init(void)
{
    memset(&g_ota_ec, 0, sizeof(g_ota_ec));
    g_ota_ec.state = OTA_EC_ST_IDLE;
    g_ota_ec.state_ts = ABox_PortGetTickMs();
    g_ota_ec.cme_error_code = -1;
    g_ota_ec.csq_rssi = -1;
    g_ota_ec.cereg_n = -1;
    g_ota_ec.cereg_stat = -1;
}

void OTA_EC_ClearErrors(void)
{
    g_ota_ec.got_error = 0U;
    g_ota_ec.cme_error = 0U;
    g_ota_ec.cme_error_code = -1;
}

void OTA_EC_SendAT(const char *cmd)
{
    char buf[256];
    int n;

    if (!cmd) return;
    n = snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    if (n > 0 && ABox_PortUartWrite((const uint8_t *)buf, (uint32_t)n))
        ABox_PortLog("[EC] TX: %s\r\n", cmd);
}

void OTA_EC_SendAT_F(const char *fmt, ...)
{
    char cmd[256];
    va_list ap;

    if (!fmt) return;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    OTA_EC_SendAT(cmd);
}

void OTA_EC_AT_OnRx(uint8_t *data, uint16_t len)
{
    uint16_t i = 0U;

    if ((data == 0) || (len == 0U)) return;
    while (i < len)
    {
        if (ABox_PortOtaIsReadingRaw())
        {
            if (g_raw_hook) g_raw_hook(&data[i], (uint16_t)(len - i));
            return;
        }

        {
            char c = (char)data[i++];
            if ((c == '\r') || (c == '\n'))
            {
                if (g_ota_ec.line_len > 0U)
                {
                    g_ota_ec.line_buf[g_ota_ec.line_len] = '\0';
                    OTA_EC_OnLine(g_ota_ec.line_buf);
                    g_ota_ec.line_len = 0U;
                    if (ABox_PortOtaIsReadingRaw())
                    {
                        while ((i < len) && ((data[i] == '\r') || (data[i] == '\n'))) i++;
                        if ((i < len) && g_raw_hook) g_raw_hook(&data[i], (uint16_t)(len - i));
                        return;
                    }
                }
            }
            else if (g_ota_ec.line_len < (OTA_EC_LINE_BUF_SIZE - 1U))
                g_ota_ec.line_buf[g_ota_ec.line_len++] = c;
            else
                g_ota_ec.line_len = 0U;
        }
    }
}

uint8_t OTA_EC_TestAT(void)
{
    OTA_EC_ClearErrors();
    g_ota_ec.got_ok = 0U;
    g_ota_ec.echo_off_done = 0U;
    g_ota_ec.state = OTA_EC_ST_WAIT_AT_RSP;
    g_ota_ec.state_ts = ABox_PortGetTickMs();
    OTA_EC_SendAT("AT");
    return 1U;
}

void OTA_EC_RequestCpin(void) { OTA_EC_ClearErrors(); g_ota_ec.cpin_done = 0U; g_ota_ec.sim_ready = 0U; OTA_EC_SendAT("AT+CPIN?"); }
void OTA_EC_RequestCsq(void) { OTA_EC_ClearErrors(); g_ota_ec.csq_done = 0U; g_ota_ec.csq_rssi = -1; OTA_EC_SendAT("AT+CSQ"); }
void OTA_EC_RequestCereg(void) { OTA_EC_ClearErrors(); g_ota_ec.cereg_done = 0U; g_ota_ec.cereg_n = -1; g_ota_ec.cereg_stat = -1; g_ota_ec.net_ready = 0U; OTA_EC_SendAT("AT+CEREG?"); }
void OTA_EC_RequestEchoOff(void) { OTA_EC_ClearErrors(); g_ota_ec.echo_off_done = 0U; OTA_EC_SendAT("ATE0"); }

void OTA_EC_AT_Task(void)
{
    uint32_t now = ABox_PortGetTickMs();
    switch (g_ota_ec.state)
    {
        case OTA_EC_ST_IDLE: break;
        case OTA_EC_ST_WAIT_AT_RSP:
            if (g_ota_ec.got_ok) { g_ota_ec.state = OTA_EC_ST_READY; g_ota_ec.state_ts = now; }
            else if (g_ota_ec.got_error || ((now - g_ota_ec.state_ts) >= 3000U)) g_ota_ec.state = OTA_EC_ST_IDLE;
            break;
        case OTA_EC_ST_READY: break;
        default: g_ota_ec.state = OTA_EC_ST_IDLE; break;
    }
}

uint8_t OTA_EC_IsReady(void) { return (g_ota_ec.state == OTA_EC_ST_READY) ? 1U : 0U; }
uint8_t OTA_EC_IsSimReady(void) { return g_ota_ec.sim_ready; }
uint8_t OTA_EC_IsCpinDone(void) { return g_ota_ec.cpin_done; }
uint8_t OTA_EC_IsCsqDone(void) { return g_ota_ec.csq_done; }
int OTA_EC_GetCsq(void) { return g_ota_ec.csq_rssi; }
uint8_t OTA_EC_IsCeregDone(void) { return g_ota_ec.cereg_done; }
uint8_t OTA_EC_IsNetReady(void) { return g_ota_ec.net_ready; }
int OTA_EC_GetCeregStat(void) { return g_ota_ec.cereg_stat; }
uint8_t OTA_EC_HasCmeError(void) { return g_ota_ec.cme_error; }
int OTA_EC_GetCmeErrorCode(void) { return g_ota_ec.cme_error_code; }
uint8_t OTA_EC_IsEchoOffDone(void) { return g_ota_ec.echo_off_done; }
void OTA_EC_RegisterLineHook(void (*hook)(const char *line)) { g_line_hook = hook; }
void OTA_EC_RegisterRawHook(void (*hook)(const uint8_t *data, uint16_t len)) { g_raw_hook = hook; }
