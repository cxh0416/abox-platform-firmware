#include "abox_ec800_command_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
static uint32_t now, writes;
static int fail_write;
static uint32_t tick(void *u) { (void)u; return now; }
static int write_data(void *u, const uint8_t *d, uint16_t n, uint32_t timeout)
{ (void)u; (void)d; (void)n; (void)timeout; ++writes; return !fail_write; }
static void feed(ABoxEc800At *at, const char *s)
{ ABoxEc800At_Feed(at, (const uint8_t *)s, (uint16_t)strlen(s)); }
int main(void)
{
    ABoxEc800At at;
    ABoxEc800AtPort transport = {0, tick, write_data, 0, 0};
    ABoxEc800CommandAdapter a;
    ABoxEc800CommandPort p;
    uint64_t op, stale;
    uint32_t count;
    CHECK(ABoxEc800At_Init(&at, &transport));
    CHECK(ABoxEc800CommandAdapter_Init(&a, &at, 19, &p));
    CHECK(p.submit(p.context, "AT+QSSLCFG=?", 10, &op));
    stale = op; CHECK(p.cancel(p.context, op)); /* Queued: no bytes sent. */
    CHECK(!writes);
    CHECK(p.submit(p.context, "AT+QSSLCFG=?", 10, &op)); CHECK(op != stale);
    CHECK(p.poll(p.context, stale) == ABOX_ASYNC_ERROR);
    ABoxEc800At_Task(&at); feed(&at, "+QMTRECV: 0,1,\"topic\",\"value\"\r\n");
    CHECK(p.poll(p.context, op) == ABOX_ASYNC_PENDING);
    feed(&at, "OK\r\n"); CHECK(p.poll(p.context, op) == ABOX_ASYNC_OK);
    CHECK(p.submit(p.context, "AT+QSSLCFG=?", 10, &op)); ABoxEc800At_Task(&at);
    CHECK(ABoxEc800At_Submit(&at, "AT+QHTTPCFG?", 2, 0, 100, 0, 0));
    count = writes; now += 10; ABoxEc800At_Task(&at);
    CHECK(at.quarantined && writes == count);
    CHECK(p.poll(p.context, op) == ABOX_ASYNC_QUARANTINED);
    feed(&at, "OK\r\n"); ABoxEc800At_Task(&at); CHECK(writes == count);
    CHECK(!ABoxEc800At_Submit(&at, "AT", 2, 0, 10, 0, 0));
    ABoxEc800At_Reset(&at); ABoxEc800CommandAdapter_OnModemReset(&a);
    CHECK(p.submit(p.context, "AT", 100, &op)); ABoxEc800At_Task(&at);
    CHECK(!p.cancel(p.context, op) && at.quarantined);
    ABoxEc800At_Reset(&at); ABoxEc800CommandAdapter_OnModemReset(&a);
    fail_write = 1;
    CHECK(p.submit(p.context, "AT", 100, &op)); ABoxEc800At_Task(&at);
    CHECK(at.quarantined && p.poll(p.context, op) == ABOX_ASYNC_QUARANTINED);
    puts("command port: completion correlation, URC, timeout, cancellation and partial-write quarantine passed");
    return 0;
}
