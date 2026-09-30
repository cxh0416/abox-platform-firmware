#include "abox_app_service.h"
#include <assert.h>
#include <string.h>
static char trace[32];
static unsigned used;
static int enabled = 1, restart;
static int ota_busy, maintenance_busy, blocked, runtime_ready = 1;
static ABoxAppService service;
static void record(char c) { trace[used++] = c; trace[used] = 0; }
void EC_Power_Task(void) { record('P'); }
int EC_Power_IsOnDone(void) { return 1; }
void EC_Power_StartRestartSequence(void) { record('Z'); }
int ABoxBootV2App_IsBusy(void) { return ota_busy; }
void ABoxEc800At_Feed(ABoxEc800At *at, const uint8_t *data, uint16_t length)
{ (void)at; (void)data; (void)length; }
void ABoxEc800At_Task(ABoxEc800At *at) { (void)at; record('A'); }
void ABoxEc800At_SetRxOverflowCount(ABoxEc800At *at, uint32_t count) { (void)at; (void)count; }
void ABoxEc800At_AbortAll(ABoxEc800At *at, ABoxEc800Result result) { (void)at; (void)result; record('X'); }
uint32_t ABoxEc800Rx_Drain(ABoxEc800Rx *rx, ABoxEc800RxSink sink, void *context, uint32_t limit)
{ (void)rx; (void)sink; (void)context; (void)limit; record('R'); return 0; }
uint32_t ABoxEc800Rx_OverflowCount(const ABoxEc800Rx *rx) { (void)rx; return 0; }
int ABoxEc800Rx_TakeRestartRequest(ABoxEc800Rx *rx) { (void)rx; return restart; }
void ABoxBootV2App_Task(void) { record('O'); }
static void restart_rx(void *context) { (void)context; record('S'); restart = 0; }
static int gate(void *context) { (void)context; record('G'); return enabled; }
static void after(void *context, uint32_t now) { (void)context; (void)now; record('D'); }
static void before(void *context, uint32_t now) { (void)context; (void)now; record('B'); }
static void enrollment(void *context, uint32_t now) { (void)context; (void)now; record('E'); }
static void cloud(void *context, uint32_t now)
{ (void)context; record('C'); ABoxAppService_Poll(&service, now); }
static void runtime(void *context, uint32_t now) { (void)context; (void)now; record('U'); }
static void transport(void *context, uint32_t now) { (void)context; (void)now; record('T'); }
static int is_runtime_ready(void *context) { (void)context; return runtime_ready; }
static int is_blocked(void *context) { (void)context; return blocked; }
static int is_maintenance_busy(void *context) { (void)context; return maintenance_busy; }
static void event(void *context, ABoxAppServiceEvent value)
{ (void)context; assert(value == ABOX_APP_NETWORK_CHANGED); record('N'); }
int main(void)
{
    ABoxEc800At at;
    ABoxEc800Rx rx;
    ABoxAppServicePort port = {.restart_rx=restart_rx, .enabled=gate, .after_at=after,
        .before_enrollment=before, .enrollment=enrollment, .cloud=cloud};
    assert(ABoxAppService_Init(&service, &at, &rx, &port));
    ABoxAppService_Poll(&service, 1U);
    assert(strcmp(trace, "PRGADOBEC") == 0); /* recursive Cloud poll must be suppressed */
    used = 0; enabled = 0; restart = 1;
    ABoxAppService_Poll(&service, 2U);
    assert(strcmp(trace, "PRXSG") == 0); /* RX recovers even while communication is gated */
    assert(!service.polling);
    ABoxEc800Recovery recovery;
    ABoxEc800Recovery_Init(&recovery, 120000U);
    port.runtime = runtime; port.runtime_ready = is_runtime_ready;
    port.transport = transport; port.transport_error = is_blocked;
    port.maintenance_busy = is_maintenance_busy; port.recovery = &recovery;
    assert(!ABoxAppService_Init(&service, &at, &rx, &port));
    port.event = event;
    assert(ABoxAppService_Init(&service, &at, &rx, &port));
    used=0; enabled=1; blocked=1; ota_busy=1;
    ABoxAppService_Poll(&service, 3U);
    assert(strcmp(trace, "PRGADOBEUTC") == 0); /* OTA owns resources: no reset */
    used=0; ota_busy=0; maintenance_busy=1;
    ABoxAppService_Poll(&service, 4U);
    assert(strcmp(trace, "PRGADOBEUTC") == 0); /* Includes failed restore lock */
    used=0; maintenance_busy=0;
    ABoxAppService_Poll(&service, 5U);
    assert(strcmp(trace, "PRGADOBEUTNZ") == 0); /* invalidate before rail restart */
    used=0;
    ABoxAppService_Poll(&service, 6U);
    assert(strcmp(trace, "PRGADOBEUTC") == 0); /* recovery cooldown */
    used=0; blocked=0; runtime_ready=0;
    ABoxAppService_Poll(&service, 7U);
    assert(strcmp(trace, "PRGADOBEU") == 0); /* runtime gates transport */
    used=0; runtime_ready=1; port.transport_first=1;
    ABoxAppService_Poll(&service, 8U);
    assert(strcmp(trace, "PRGADOBETUC") == 0); /* baremetal compatibility order */
    return 0;
}
