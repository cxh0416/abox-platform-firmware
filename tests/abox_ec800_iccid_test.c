#include "abox_ec800_iccid.h"
#include <assert.h>
#include <string.h>

static uint32_t tick_ms(void *user) { (void)user; return 0U; }
static int write_data(void *user, const uint8_t *data, uint16_t length,
                      uint32_t timeout_ms)
{
    (void)user;
    (void)timeout_ms;
    return length == 10U && memcmp(data, "AT+QCCID\r\n", 10U) == 0;
}
static void feed(ABoxEc800At *at, const char *line)
{
    ABoxEc800At_Feed(at, (const uint8_t *)line, (uint16_t)strlen(line));
}
int main(void)
{
    ABoxEc800At at;
    ABoxEc800Iccid identity;
    ABoxEc800AtPort port = {0};
    port.tick_ms = tick_ms;
    port.write = write_data;
    assert(ABoxEc800At_Init(&at, &port));
    assert(ABoxEc800Iccid_Init(&identity, &at, ABOX_EC800_OWNER_PRODUCT_BASE + 5U));
    assert(ABoxEc800Iccid_Value(&identity) == 0);
    assert(ABoxEc800Iccid_Request(&identity, 100U));
    assert(!ABoxEc800Iccid_Request(&identity, 101U));
    ABoxEc800At_Task(&at);
    feed(&at, "+QCCID: 89860012345678901234\r\nOK\r\n");
    assert(identity.valid && !identity.pending);
    assert(strcmp(ABoxEc800Iccid_Value(&identity), "89860012345678901234") == 0);
    ABoxEc800Iccid_OnModemReset(&identity);
    assert(!identity.valid && ABoxEc800Iccid_Value(&identity) == 0);
    assert(ABoxEc800Iccid_Request(&identity, 0U));
    ABoxEc800Iccid_Poll(&identity, 5001U);
    assert(!identity.pending);
    ABoxEc800At_Task(&at);
    feed(&at, "ERROR\r\n");
    assert(ABoxEc800Iccid_Value(&identity) == 0);
    return 0;
}
