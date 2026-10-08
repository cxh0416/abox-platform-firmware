#include "abox_psk_station.h"
#include <assert.h>
#include <string.h>
static unsigned calls;
static int stage(const ABoxPskStationInput *input)
{
    ++calls;
    assert(input->credentials.generation == 1 && input->port == 26443);
    return 1;
}
int main(void)
{
    ABoxPskStation_Input.credentials.mode = ABOX_TLS_AUTH_PSK;
    ABoxPskStation_Input.credentials.generation = 1;
    strcpy(ABoxPskStation_Input.credentials.identity, "fixture.g1");
    strcpy(ABoxPskStation_Input.credentials.secret, "abcdefghijklmnopqrstuvwxyz012345");
    ABoxPskStation_Input.port = 26443;
    ABoxPskStation_Request = 1;
    ABoxPskStation_Poll(stage);
    assert(calls == 1 && ABoxPskStation_Completed == 1 && ABoxPskStation_Result == 1);
    assert(ABoxPskStation_Input.credentials.secret[0] == 0);
    ABoxPskStation_Poll(stage);
    assert(calls == 1);
    ABoxPskStation_Request = 2; /* Invalid input rejected and cleared. */
    ABoxPskStation_Poll(stage);
    assert(calls == 1 && ABoxPskStation_Completed == 2 && ABoxPskStation_Result == 0);
    return 0;
}
