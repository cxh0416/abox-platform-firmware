#include "abox_psk_station.h"
#include <string.h>
volatile uint32_t ABoxPskStation_Request, ABoxPskStation_Completed, ABoxPskStation_Result;
ABoxPskStationInput *ABoxPskStation_Input;
void ABoxPskStation_SetInputBuffer(void *storage, size_t capacity)
{
    ABoxPskStation_Input = capacity >= sizeof(*ABoxPskStation_Input) ? storage : NULL;
    if (ABoxPskStation_Input) memset(ABoxPskStation_Input, 0, sizeof(*ABoxPskStation_Input));
}
void ABoxPskStation_Poll(int (*stage)(const ABoxPskStationInput *))
{
    if (!ABoxPskStation_Input || ABoxPskStation_Request == ABoxPskStation_Completed) return;
    ABoxPskStation_Result = 0;
    if (stage && ABoxTlsCredentials_Valid(&ABoxPskStation_Input->credentials) &&
        ABoxPskStation_Input->credentials.mode == ABOX_TLS_AUTH_PSK &&
        ABoxPskStation_Input->port &&
        memchr(ABoxPskStation_Input->host, 0, sizeof(ABoxPskStation_Input->host)) &&
        memchr(ABoxPskStation_Input->username, 0, sizeof(ABoxPskStation_Input->username)) &&
        memchr(ABoxPskStation_Input->password, 0, sizeof(ABoxPskStation_Input->password)))
        ABoxPskStation_Result = stage(ABoxPskStation_Input) ? 1U : 0U;
    memset(ABoxPskStation_Input, 0, sizeof(*ABoxPskStation_Input));
    ABoxPskStation_Completed = ABoxPskStation_Request;
}
