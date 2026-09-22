#include "abox_ec800_tls.h"
#include <stdio.h>
#include <string.h>
static ABoxTlsSlot *slot(ABoxEc800Tls *t, ABoxTlsLease l)
{
    if (!t || l.context >= ABOX_TLS_CONTEXT_CAPACITY || !l.generation ||
        t->slots[l.context].generation != l.generation || !t->slots[l.context].owner) return NULL;
    return &t->slots[l.context];
}
int ABoxEc800Tls_Init(ABoxEc800Tls *t, const ABoxEc800CommandPort *p, uint8_t mask)
{
    if (!t || !p || !p->submit || !p->poll || !p->cancel) return 0;
    memset(t, 0, sizeof(*t)); t->port = *p;
    t->supported_mask = (uint8_t)(mask & ~(1U << ABOX_TLS_LEGACY_HTTPS_CONTEXT)); return 1;
}
int ABoxEc800Tls_Acquire(ABoxEc800Tls *t, uint8_t id, uint32_t owner, ABoxTlsLease *l)
{
    ABoxTlsSlot *s;
    if (!t || !l || !owner || id >= ABOX_TLS_CONTEXT_CAPACITY ||
        !(t->supported_mask & (1U << id)) || t->slots[id].owner || t->serial == UINT64_MAX) return 0;
    s = &t->slots[id]; memset(s, 0, sizeof(*s));
    s->owner = owner; s->generation = ++t->serial;
    l->context = id; l->generation = s->generation; return 1;
}
int ABoxEc800Tls_Release(ABoxEc800Tls *t, ABoxTlsLease l)
{
    ABoxTlsSlot *s = slot(t, l);
    if (!s || s->pins || s->status == ABOX_ASYNC_PENDING ||
        s->status == ABOX_ASYNC_QUARANTINED) return 0;
    memset(s, 0, sizeof(*s)); return 1;
}
int ABoxEc800Tls_Prepare(ABoxEc800Tls *t, ABoxTlsLease l,
                         const ABoxTlsProfile *p, uint32_t now, uint32_t timeout)
{
    ABoxTlsSlot *s = slot(t, l);
    if (!s || !p || !p->ca_verified || !p->time_valid || !p->ca_revision ||
        !ABoxEc800Ufs_PathValid(p->ca_file) || !timeout || timeout > INT32_MAX ||
        s->status != ABOX_ASYNC_IDLE || s->pins) return 0;
    strcpy(s->ca_file, p->ca_file); s->revision = p->ca_revision;
    s->started = now; s->timeout = timeout; s->status = ABOX_ASYNC_PENDING; return 1;
}
static void stop(ABoxEc800Tls *t, ABoxTlsSlot *s, ABoxAsyncStatus reason)
{
    if (s->waiting && !t->port.cancel(t->port.context, s->operation))
        s->status = ABOX_ASYNC_QUARANTINED;
    else s->status = reason;
    s->waiting = 0;
}
void ABoxEc800Tls_Poll(ABoxEc800Tls *t, uint32_t now)
{
    uint8_t id;
    if (!t) return;
    for (id = 0; id < ABOX_TLS_CONTEXT_CAPACITY; ++id) {
        ABoxTlsSlot *s = &t->slots[id];
        char command[160];
        if (s->status != ABOX_ASYNC_PENDING) continue;
        if ((uint32_t)(now - s->started) >= s->timeout) { stop(t, s, ABOX_ASYNC_TIMEOUT); continue; }
        if (s->waiting) {
            ABoxAsyncStatus result = t->port.poll(t->port.context, s->operation);
            if (result == ABOX_ASYNC_PENDING) continue;
            if (result != ABOX_ASYNC_OK) { stop(t, s, ABOX_ASYNC_ERROR); continue; }
            s->waiting = 0;
            if (++s->step == 5) { s->status = ABOX_ASYNC_OK; continue; }
        }
        switch (s->step) {
        case 0: snprintf(command, sizeof(command), "AT+QSSLCFG=\"sslversion\",%u,3", id); break;
        case 1: snprintf(command, sizeof(command), "AT+QSSLCFG=\"seclevel\",%u,1", id); break;
        case 2: snprintf(command, sizeof(command), "AT+QSSLCFG=\"sni\",%u,1", id); break;
        case 3: snprintf(command, sizeof(command), "AT+QSSLCFG=\"ignorelocaltime\",%u,0", id); break;
        default: snprintf(command, sizeof(command), "AT+QSSLCFG=\"cacert\",%u,\"%s\"", id, s->ca_file); break;
        }
        /* Rejected enqueue is retried until the overall deadline. */
        s->waiting = (uint8_t)!!t->port.submit(t->port.context, command,
                     s->timeout - (uint32_t)(now - s->started), &s->operation);
    }
}
ABoxAsyncStatus ABoxEc800Tls_Status(const ABoxEc800Tls *t, ABoxTlsLease l)
{
    if (!t || l.context >= ABOX_TLS_CONTEXT_CAPACITY || !l.generation ||
        !t->slots[l.context].owner || t->slots[l.context].generation != l.generation) return ABOX_ASYNC_ERROR;
    return t->slots[l.context].status;
}
int ABoxEc800Tls_Pin(ABoxEc800Tls *t, ABoxTlsLease l)
{
    ABoxTlsSlot *s = slot(t, l);
    if (!s || s->status != ABOX_ASYNC_OK || s->pins == UINT16_MAX) return 0;
    ++s->pins; return 1;
}
int ABoxEc800Tls_Unpin(ABoxEc800Tls *t, ABoxTlsLease l)
{
    ABoxTlsSlot *s = slot(t, l);
    if (!s || !s->pins) return 0;
    --s->pins; return 1;
}
void ABoxEc800Tls_Cancel(ABoxEc800Tls *t, ABoxTlsLease l)
{ ABoxTlsSlot *s = slot(t, l); if (s && s->status == ABOX_ASYNC_PENDING) stop(t, s, ABOX_ASYNC_CANCELLED); }
void ABoxEc800Tls_OnModemReset(ABoxEc800Tls *t)
{ if (t) memset(t->slots, 0, sizeof(t->slots)); }
