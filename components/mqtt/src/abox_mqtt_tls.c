#include "abox_mqtt_tls.h"
#include <stdio.h>
#include <string.h>
int ABoxMqttTls_Init(ABoxMqttTls *m, ABoxEc800Tls *t,
                     const ABoxEc800CommandPort *p, uint8_t client)
{
    if (!m || !t || !p || !p->submit || !p->poll || !p->cancel) return 0;
    memset(m, 0, sizeof(*m)); m->tls = t; m->port = *p; m->client = client; return 1;
}
static int begin(ABoxMqttTls *m, int enabled, uint32_t now, uint32_t timeout)
{
    if (!timeout || timeout > INT32_MAX || m->status == ABOX_ASYNC_PENDING ||
        m->status == ABOX_ASYNC_QUARANTINED) return 0;
    m->enabled = (uint8_t)!!enabled; m->started = now; m->timeout = timeout;
    m->waiting = 0; m->status = ABOX_ASYNC_PENDING; return 1;
}
int ABoxMqttTls_Start(ABoxMqttTls *m, int enabled, ABoxTlsLease l,
                      uint32_t now, uint32_t timeout)
{
    if (!m || !m->tls || m->pinned || (enabled != 0 && enabled != 1) ||
        !timeout || timeout > INT32_MAX || m->status == ABOX_ASYNC_PENDING ||
        m->status == ABOX_ASYNC_QUARANTINED) return 0;
    if (enabled && !ABoxEc800Tls_Pin(m->tls, l)) return 0;
    m->lease = l; m->pinned = (uint8_t)enabled;
    return begin(m, enabled, now, timeout);
}
int ABoxMqttTls_Unbind(ABoxMqttTls *m, uint32_t now, uint32_t timeout)
{ return m && m->tls && begin(m, 0, now, timeout); }
static void stop(ABoxMqttTls *m, ABoxAsyncStatus reason)
{
    m->status = (m->waiting && !m->port.cancel(m->port.context, m->operation)) ?
                ABOX_ASYNC_QUARANTINED : reason;
    m->waiting = 0;
    /* A failed binding may already be effective in the modem: retain pin. */
}
void ABoxMqttTls_Poll(ABoxMqttTls *m, uint32_t now)
{
    char command[64];
    if (!m) return;
    if (m->pinned && ABoxEc800Tls_Status(m->tls, m->lease) != ABOX_ASYNC_OK) {
        if (m->status == ABOX_ASYNC_PENDING) stop(m, ABOX_ASYNC_ERROR);
        else m->status = ABOX_ASYNC_ERROR;
        return;
    }
    if (m->status != ABOX_ASYNC_PENDING) return;
    if ((uint32_t)(now - m->started) >= m->timeout) { stop(m, ABOX_ASYNC_TIMEOUT); return; }
    if (m->waiting) {
        ABoxAsyncStatus status = m->port.poll(m->port.context, m->operation);
        if (status == ABOX_ASYNC_PENDING) return;
        if (status != ABOX_ASYNC_OK) { stop(m, ABOX_ASYNC_ERROR); return; }
        m->waiting = 0; m->status = ABOX_ASYNC_OK;
        if (!m->enabled && m->pinned) {
            (void)ABoxEc800Tls_Unpin(m->tls, m->lease); m->pinned = 0;
        }
        return;
    }
    if (m->enabled) snprintf(command, sizeof(command), "AT+QMTCFG=\"SSL\",%u,1,%u", m->client, m->lease.context);
    else snprintf(command, sizeof(command), "AT+QMTCFG=\"SSL\",%u,0", m->client);
    m->waiting = (uint8_t)!!m->port.submit(m->port.context, command,
                 m->timeout - (uint32_t)(now - m->started), &m->operation);
}
void ABoxMqttTls_Cancel(ABoxMqttTls *m)
{ if (m && m->status == ABOX_ASYNC_PENDING) stop(m, ABOX_ASYNC_CANCELLED); }
void ABoxMqttTls_OnModemReset(ABoxMqttTls *m)
{ if (m) { m->pinned = m->waiting = m->enabled = 0; m->status = ABOX_ASYNC_IDLE; } }
