#include "ec800_async_fixture.h"
int main(void)
{
    MockCommand m;
    ABoxEc800CommandPort p = mock_port(&m);
    ABoxEc800Tls t;
    ABoxTlsLease l, other, stale;
    ABoxTlsProfile profile = {"UFS:mqtt_ca_v2.pem", 2, 1, 1};
    unsigned i;
    CHECK(ABoxEc800Tls_Init(&t, &p, 0x06));
    CHECK(!ABoxEc800Tls_Acquire(&t, 1, 16, &l)); /* Legacy OTA reservation. */
    CHECK(!ABoxEc800Tls_Acquire(&t, 0, 16, &l)); /* Not verified supported. */
    CHECK(!ABoxEc800Tls_Acquire(&t, 8, 16, &l));
    l = prepared(&t, 2);
    CHECK(!ABoxEc800Tls_Acquire(&t, 2, 16, &other)); /* Same owner also conflicts. */
    CHECK(!ABoxEc800Tls_Acquire(&t, 2, 17, &other));
    CHECK(m.count == 5);
    CHECK(!strcmp(m.commands[0], "AT+QSSLCFG=\"sslversion\",2,3"));
    CHECK(!strcmp(m.commands[3], "AT+QSSLCFG=\"ignorelocaltime\",2,0"));
    CHECK(!strcmp(m.commands[4], "AT+QSSLCFG=\"cacert\",2,\"UFS:mqtt_ca_v1.pem\""));
    CHECK(ABoxEc800Tls_Pin(&t, l));
    CHECK(!ABoxEc800Tls_Release(&t, l));
    CHECK(!ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    CHECK(ABoxEc800Tls_Unpin(&t, l));
    CHECK(ABoxEc800Tls_Release(&t, l)); stale = l;
    CHECK(ABoxEc800Tls_Acquire(&t, 2, 17, &l));
    CHECK(!ABoxEc800Tls_Release(&t, stale));
    profile.ca_verified = 0;
    CHECK(!ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    profile.ca_verified = 1; profile.time_valid = 0;
    CHECK(!ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    profile.time_valid = 1; profile.ca_file = "UFS:bad\"\r\n";
    CHECK(!ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    profile.ca_file = "UFS:ca.pem";
    CHECK(ABoxEc800Tls_Prepare(&t, l, &profile, UINT32_MAX - 10, 20));
    m.result = ABOX_ASYNC_PENDING; m.drain = 0;
    ABoxEc800Tls_Poll(&t, UINT32_MAX - 10);
    CHECK(!ABoxEc800Tls_Release(&t, l));
    ABoxEc800Tls_Poll(&t, 9);
    CHECK(ABoxEc800Tls_Status(&t, l) == ABOX_ASYNC_QUARANTINED);
    CHECK(!ABoxEc800Tls_Release(&t, l));
    /* Late completion cannot turn a quarantined slot ready. */
    m.result = ABOX_ASYNC_OK; ABoxEc800Tls_Poll(&t, 10);
    CHECK(ABoxEc800Tls_Status(&t, l) == ABOX_ASYNC_QUARANTINED);
    m.current = 0; ABoxEc800Tls_OnModemReset(&t);
    CHECK(ABoxEc800Tls_Status(&t, l) == ABOX_ASYNC_ERROR);
    CHECK(!ABoxEc800Tls_Acquire(&t, 1, 17, &other));
    CHECK(ABoxEc800Tls_Acquire(&t, 2, 17, &other));
    CHECK(other.generation != l.generation);
    CHECK(ABoxEc800Tls_Prepare(&t, other, &profile, 0, 100));
    m.result = ABOX_ASYNC_ERROR; m.drain = 1;
    ABoxEc800Tls_Poll(&t, 0); ABoxEc800Tls_Poll(&t, 1);
    CHECK(ABoxEc800Tls_Status(&t, other) == ABOX_ASYNC_ERROR);
    CHECK(ABoxEc800Tls_Release(&t, other));
    CHECK(ABoxEc800Tls_Acquire(&t, 2, 17, &other));
    CHECK(ABoxEc800Tls_Prepare(&t, other, &profile, 0, 10));
    m.reject = 1;
    ABoxEc800Tls_Poll(&t, 0); ABoxEc800Tls_Poll(&t, 10);
    CHECK(ABoxEc800Tls_Status(&t, other) == ABOX_ASYNC_TIMEOUT);
    CHECK(ABoxEc800Tls_Release(&t, other));
    /* Error at each individual configuration step must never produce ready. */
    for (i = 0; i < 5; ++i) {
        unsigned j;
        m.reject = 0; m.result = ABOX_ASYNC_OK;
        CHECK(ABoxEc800Tls_Acquire(&t, 2, 17, &other));
        CHECK(ABoxEc800Tls_Prepare(&t, other, &profile, 0, 100));
        ABoxEc800Tls_Poll(&t, 0);
        for (j = 0; j < i; ++j) ABoxEc800Tls_Poll(&t, j + 1);
        m.result = ABOX_ASYNC_ERROR; ABoxEc800Tls_Poll(&t, 8);
        CHECK(ABoxEc800Tls_Status(&t, other) == ABOX_ASYNC_ERROR);
        CHECK(ABoxEc800Tls_Release(&t, other));
    }
    /* Two independent contexts share a single serialized command transport. */
    p = mock_port(&m);
    CHECK(ABoxEc800Tls_Init(&t, &p, 0x0e));
    CHECK(ABoxEc800Tls_Acquire(&t, 2, 16, &l));
    CHECK(ABoxEc800Tls_Acquire(&t, 3, 17, &other));
    CHECK(ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    CHECK(ABoxEc800Tls_Prepare(&t, other, &profile, 0, 100));
    for (i = 0; i < 12; ++i) ABoxEc800Tls_Poll(&t, i);
    CHECK(ABoxEc800Tls_Status(&t, l) == ABOX_ASYNC_OK);
    CHECK(ABoxEc800Tls_Status(&t, other) == ABOX_ASYNC_OK);
    CHECK(m.count == 10);
    CHECK(ABoxEc800Tls_Release(&t, l));
    CHECK(ABoxEc800Tls_Acquire(&t, 2, 16, &l));
    CHECK(ABoxEc800Tls_Prepare(&t, l, &profile, 0, 100));
    m.result = ABOX_ASYNC_PENDING; ABoxEc800Tls_Poll(&t, 0);
    ABoxEc800Tls_Cancel(&t, l);
    CHECK(ABoxEc800Tls_Status(&t, l) == ABOX_ASYNC_CANCELLED);
    CHECK(ABoxEc800Tls_Status(&t, other) == ABOX_ASYNC_OK);
    CHECK(ABoxEc800Tls_Release(&t, l));
    puts("TLS: reservation, ownership, pins, stale leases, reset, deadline and per-step failures passed");
    return 0;
}
