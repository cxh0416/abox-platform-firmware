#include "abox_app_service.h"
#include "abox_boot_v2_app.h"
#include "ec_power.h"
#include <string.h>
static void receive(void *context, const uint8_t *data, uint16_t length)
{ ABoxEc800At_Feed((ABoxEc800At *)context, data, length); }
int ABoxAppService_Init(ABoxAppService *service, ABoxEc800At *at,
    ABoxEc800Rx *rx, const ABoxAppServicePort *port)
{
    if (!service || !at || !port || !port->cloud || (rx && !port->restart_rx)) return 0;
    memset(service, 0, sizeof(*service));
    service->at = at; service->rx = rx; service->port = port;
    return 1;
}
void ABoxAppService_Event(ABoxAppService *service, ABoxAppServiceEvent event)
{
    if (service && service->port && service->port->event)
        service->port->event(service->port->context, event);
}
void ABoxAppService_Poll(ABoxAppService *service, uint32_t now)
{
    const ABoxAppServicePort *p;
    if (!service || !service->at || !service->port || service->polling) return;
    service->polling = 1U;
    p = service->port;
    EC_Power_Task();
    if (service->rx) {
        (void)ABoxEc800Rx_Drain(service->rx, receive, service->at, 0U);
        ABoxEc800At_SetRxOverflowCount(service->at, ABoxEc800Rx_OverflowCount(service->rx));
        if (ABoxEc800Rx_TakeRestartRequest(service->rx)) {
            ABoxEc800At_AbortAll(service->at, ABOX_EC800_RESULT_ERROR);
            p->restart_rx(p->context);
        }
    }
    if (!p->enabled || p->enabled(p->context)) {
        ABoxEc800At_Task(service->at);
        if (p->after_at) p->after_at(p->context, now);
        ABoxBootV2App_Task();
        if (p->before_enrollment) p->before_enrollment(p->context, now);
        if (p->enrollment) p->enrollment(p->context, now);
        p->cloud(p->context, now);
    }
    service->polling = 0U;
}
