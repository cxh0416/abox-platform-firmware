#include "abox_ec800_diagnostics.h"
#include <assert.h>
#include <string.h>

static uint32_t now;
static char command[256];
static unsigned writes;
static uint32_t tick(void *u) { (void)u; return now; }
static int write_bytes(void *u, const uint8_t *data, uint16_t n, uint32_t timeout)
{ (void)u; (void)timeout; memcpy(command, data, n); command[n] = 0; ++writes; return 1; }
static void feed(ABoxEc800At *at, const char *s)
{ ABoxEc800At_Feed(at, (const uint8_t *)s, (uint16_t)strlen(s)); }
static void query(ABoxEc800Diagnostics *d, const char *cmd, const char *reply)
{
    ABoxEc800Diagnostics_Poll(d, now, 1, 0, 0, 0);
    ABoxEc800At_Task(d->at);
    assert(strstr(command, cmd));
    feed(d->at, reply);
}
int main(void)
{
    ABoxEc800At at;
    ABoxEc800Diagnostics d;
    ABoxEc800DiagSnapshot s;
    ABoxEc800AtPort port = {0};
    port.tick_ms = tick; port.write = write_bytes;
    assert(ABoxEc800At_Init(&at, &port));
    assert(ABoxEc800Diagnostics_Init(&d, &at, 22U));
    query(&d, "AT+CMEE=1", "\r\nOK\r\n");
    query(&d, "AT+CPIN?", "\r\n+CME ERROR: 10\r\n");
    assert(d.snapshot.valid && d.snapshot.state == ABOX_EC800_DIAG_SIM_NOT_INSERTED);
    assert(d.snapshot.last_cme == 10);
    now = 10000U; ABoxEc800Diagnostics_Poll(&d, now, 1, 0, 0, 0);
    assert(writes == 2 && !d.pending); /* No rapid retry or restart. */
    now = 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: SIM PIN\r\nOK\r\n");
    assert(d.snapshot.state == ABOX_EC800_DIAG_SIM_PIN_REQUIRED);
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: SIM PUK\r\nOK\r\n");
    assert(d.snapshot.state == ABOX_EC800_DIAG_SIM_PIN_REQUIRED);
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CME ERROR: 13\r\n");
    assert(d.snapshot.state == ABOX_EC800_DIAG_SIM_ERROR);
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: NOT READY\r\nOK\r\n");
    assert(!d.snapshot.valid && d.snapshot.state == ABOX_EC800_DIAG_UNKNOWN);
    now += 15000U; query(&d, "AT+CPIN?", "\r\nERROR\r\n");
    assert(!d.snapshot.valid); /* Generic ERROR is not absence. */
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: READY\r\nOK\r\n");
    query(&d, "AT+CEREG?", "\r\n+CEREG: 0,2\r\nOK\r\n");
    ABoxEc800Diagnostics_Poll(&d, now, 0, 0, 0, 0);
    assert(d.snapshot.state == ABOX_EC800_DIAG_NETWORK_NOT_REGISTERED);
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: READY\r\nOK\r\n");
    query(&d, "AT+CEREG?", "\r\n+CEREG: 0,5\r\nOK\r\n");
    ABoxEc800Diagnostics_Poll(&d, now, 0, 1, 0, 0);
    assert(d.snapshot.state == ABOX_EC800_DIAG_DATA_NOT_READY);
    ABoxEc800Diagnostics_Poll(&d, now, 0, 1, 1, 1);
    assert(d.snapshot.state == ABOX_EC800_DIAG_MQTT_READY);
    ABoxEc800Diagnostics_Get(&d, &s); assert(s.updated_at_ms == now);
    /* Enrollment/OTA/MQTT queue always wins. No diagnostic command added. */
    for (unsigned owner = 1U; owner < 4U; ++owner) {
        now += 15000U;
        assert(ABoxEc800At_Submit(&at, "AT", (ABoxEc800Owner)owner,
                                 ABOX_EC800_PRIORITY_HIGH, 1000U, 0, 0));
        ABoxEc800Diagnostics_Poll(&d, now, 1, 0, 0, 0); assert(!d.pending);
        ABoxEc800At_Task(&at); feed(&at, "\r\nOK\r\n");
    }
    query(&d, "AT+CPIN?", ""); now += 3000U; ABoxEc800At_Task(&at);
    assert(d.snapshot.state == ABOX_EC800_DIAG_MODEM_NO_RESPONSE);
    assert(d.snapshot.valid && d.snapshot.last_result == ABOX_EC800_RESULT_TIMEOUT);
    now += 15000U; query(&d, "AT+CPIN?", "\r\n+CPIN: READY\r\nOK\r\n");
    query(&d, "AT+CEREG?", "\r\nERROR\r\n");
    assert(d.snapshot.state == ABOX_EC800_DIAG_SIM_READY);
    feed(&at, "\r\nRDY\r\n"); assert(!d.snapshot.valid && !d.pending);
    now = UINT32_MAX - 2000U;
    query(&d, "AT+CMEE=1", "\r\nOK\r\n");
    query(&d, "AT+CPIN?", "\r\n+CME ERROR: SIM not inserted\r\n");
    now = 13000U; query(&d, "AT+CPIN?", "\r\n+CPIN: READY\r\nOK\r\n");
    query(&d, "AT+CEREG?", "\r\n+CEREG: 0,1\r\nOK\r\n");
    now += 60000U; ABoxEc800Diagnostics_Poll(&d, now, 0, 0, 0, 0);
    assert(!d.snapshot.valid && d.snapshot.state == ABOX_EC800_DIAG_UNKNOWN);
    return 0;
}
