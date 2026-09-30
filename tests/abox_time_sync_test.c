#include "abox_time_sync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned sends, applies;
static uint64_t selected;
static int send(void *ctx, const char *id) { (void)ctx; assert(!strncmp(id,"ts-",3)); sends++; return 1; }
static int apply(void *ctx, uint64_t utc, uint32_t err, uint64_t sent, uint64_t recv)
{ (void)ctx; (void)err; assert(recv - sent == 100); selected = utc; applies++; return 1; }
int main(void)
{
    ABoxTimeSync s;
    ABoxTimeSyncPort p = {0, send, apply};
    const uint64_t t = 1800000000000ULL;
    char old[64];
    ABoxTimeSync_Init(&s);
    ABoxTimeSync_Poll(&s,&p,1,1000,42); strcpy(old,s.request_id);
    assert(!strcmp(old,"ts-000000000000002a-1"));
    assert(!ABoxTimeSync_Response(&s,"stale",t,10,1200) && s.pending);
    assert(ABoxTimeSync_Response(&s,s.request_id,t+1100,10,1200));
    ABoxTimeSync_Poll(&s,&p,1,2200,42);
    assert(!ABoxTimeSync_Response(&s,old,t,10,2300) && s.pending);
    assert(ABoxTimeSync_Response(&s,s.request_id,t+2250,10,2300));
    ABoxTimeSync_Poll(&s,&p,1,3300,42);
    assert(ABoxTimeSync_Response(&s,s.request_id,t+3450,10,3600));
    ABoxTimeSync_Poll(&s,&p,1,4600,42);
    assert(sends == 3 && applies == 1 && selected == t+2250 && s.next_due == 1102100);
    ABoxTimeSync_Poll(&s,&p,1,1102099,42); assert(sends == 3);
    ABoxTimeSync_Poll(&s,&p,0,1102100,42);
    ABoxTimeSync_Poll(&s,&p,1,1102101,42); assert(sends == 4);
    assert(!ABoxTimeSync_Response(&s,s.request_id,t,10,1102502)); /* RTT 401 */
    ABoxTimeSync_Poll(&s,&p,1,1103502,42);
    ABoxTimeSync_Poll(&s,&p,1,1108502,42); /* timeout -> third sample */
    ABoxTimeSync_Poll(&s,&p,1,1113502,42); /* no valid sample -> 30 s retry */
    assert(applies == 1 && s.next_due == 1143502);
    ABoxTimeSync_Init(&s);
    ABoxTimeSync_Poll(&s,&p,1,1000,42);
    assert(ABoxTimeSync_Response(&s,s.request_id,t,10,1100));
    ABoxTimeSync_Poll(&s,&p,1,2100,42);
    assert(!ABoxTimeSync_Response(&s,s.request_id,t+3000,10,2200));
    ABoxTimeSync_Poll(&s,&p,1,3200,42);
    assert(applies == 1 && !s.have_best && s.next_due == 33200);
    puts("time sync: low RTT, stale correlation, timeout, reconnect, schedule PASS");
}
