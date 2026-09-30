#include "abox_app_service.h"
#include <assert.h>
#include <string.h>
static char trace[32];
static unsigned used;
static int enabled = 1, restart;
static ABoxAppService service;
static void record(char c) { trace[used++] = c; trace[used] = 0; }
void EC_Power_Task(void) { record('P'); }
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
int main(void)
{
    ABoxEc800At at;
    ABoxEc800Rx rx;
    const ABoxAppServicePort port = {0, restart_rx, gate, after, before, enrollment, cloud, 0};
    assert(ABoxAppService_Init(&service, &at, &rx, &port));
    ABoxAppService_Poll(&service, 1U);
    assert(strcmp(trace, "PRGADOBEC") == 0); /* recursive Cloud poll must be suppressed */
    used = 0; enabled = 0; restart = 1;
    ABoxAppService_Poll(&service, 2U);
    assert(strcmp(trace, "PRXSG") == 0); /* RX recovers even while communication is gated */
    assert(!service.polling);
    return 0;
}
