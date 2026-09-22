#ifndef EC800_ASYNC_FIXTURE_H
#define EC800_ASYNC_FIXTURE_H
#include "abox_ec800_tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
typedef struct {
    char commands[64][160];
    uint64_t serial, current;
    unsigned count, cancels;
    int reject, drain;
    ABoxAsyncStatus result;
} MockCommand;
static int mock_submit(void *context, const char *cmd, uint32_t timeout, uint64_t *id)
{
    MockCommand *m = context;
    CHECK(timeout > 0);
    if (m->reject || m->current) return 0;
    CHECK(m->count < 64 && strlen(cmd) < 160);
    strcpy(m->commands[m->count++], cmd);
    *id = m->current = ++m->serial; return 1;
}
static ABoxAsyncStatus mock_poll(void *context, uint64_t id)
{
    MockCommand *m = context;
    if (id != m->current) return ABOX_ASYNC_PENDING;
    if (m->result == ABOX_ASYNC_OK) m->current = 0;
    return m->result;
}
static int mock_cancel(void *context, uint64_t id)
{
    MockCommand *m = context; ++m->cancels;
    if (m->drain && id == m->current) m->current = 0;
    return m->drain;
}
static ABoxEc800CommandPort mock_port(MockCommand *m)
{
    ABoxEc800CommandPort p = {m, mock_submit, mock_poll, mock_cancel};
    memset(m, 0, sizeof(*m)); m->drain = 1; m->result = ABOX_ASYNC_OK; return p;
}
static ABoxTlsLease prepared(ABoxEc800Tls *t, uint8_t id)
{
    ABoxTlsLease l;
    ABoxTlsProfile profile = {"UFS:mqtt_ca_v1.pem", 1, 1, 1};
    unsigned i;
    CHECK(ABoxEc800Tls_Acquire(t, id, 16, &l));
    CHECK(ABoxEc800Tls_Prepare(t, l, &profile, 0, 100));
    for (i = 0; i < 6; ++i) ABoxEc800Tls_Poll(t, i);
    CHECK(ABoxEc800Tls_Status(t, l) == ABOX_ASYNC_OK); return l;
}
#endif
