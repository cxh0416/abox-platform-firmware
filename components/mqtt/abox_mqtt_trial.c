#include "abox_mqtt_trial.h"
#include <string.h>
int ABoxMqttTrial_Init(ABoxMqttTrial *t, const ABoxMqttTrialPort *p)
{
    if (!t || !p || !p->prepare || !p->connect || !p->verify || !p->commit || !p->restore) return 0;
    memset(t, 0, sizeof(*t)); t->port = *p; return 1;
}
static int running(const ABoxMqttTrial *t)
{ return t->state >= ABOX_MQTT_TRIAL_WAIT_ACCEPT && t->state <= ABOX_MQTT_TRIAL_COMMITTING; }
static int timed(const ABoxMqttTrial *t)
{ return t->state >= ABOX_MQTT_TRIAL_PREPARING && t->state <= ABOX_MQTT_TRIAL_COMMITTING; }
int ABoxMqttTrial_Start(ABoxMqttTrial *t, const void *active, const void *candidate,
                       uint64_t proof, uint32_t now, uint32_t timeout, uint32_t restore_timeout)
{
    if (!t || !t->port.prepare || !active || !candidate || active == candidate || !proof ||
        !timeout || timeout > INT32_MAX || !restore_timeout || restore_timeout > INT32_MAX ||
        running(t) || t->state == ABOX_MQTT_TRIAL_RESTORING ||
        t->state == ABOX_MQTT_TRIAL_RESTORE_FAILED || t->serial == UINT64_MAX) return 0;
    t->active = active; t->candidate = candidate; t->proof = proof;
    t->session = ++t->serial; t->started = now; t->timeout = timeout;
    t->restore_timeout = restore_timeout; t->reason = ABOX_MQTT_TRIAL_REASON_NONE;
    t->state = ABOX_MQTT_TRIAL_WAIT_ACCEPT; return 1;
}
static void restore(ABoxMqttTrial *t, uint32_t now, ABoxMqttTrialReason reason)
{
    t->reason = reason; t->state = ABOX_MQTT_TRIAL_RESTORING; t->restore_started = now;
    if (!t->port.restore(t->port.user, t->session, t->active)) t->state = ABOX_MQTT_TRIAL_RESTORE_FAILED;
}
void ABoxMqttTrial_Poll(ABoxMqttTrial *t, uint32_t now)
{
    if (!t) return;
    if (timed(t) && (uint32_t)(now - t->started) >= t->timeout)
        restore(t, now, ABOX_MQTT_TRIAL_REASON_TIMEOUT);
    else if (t->state == ABOX_MQTT_TRIAL_RESTORING &&
             (uint32_t)(now - t->restore_started) >= t->restore_timeout)
        t->state = ABOX_MQTT_TRIAL_RESTORE_FAILED;
}
void ABoxMqttTrial_Event(ABoxMqttTrial *t, uint64_t session,
                       ABoxMqttTrialEvent event, uint64_t proof, uint32_t now)
{
    ABoxMqttTrialAction action = NULL;
    if (!t || !session || session != t->session) return;
    ABoxMqttTrial_Poll(t, now);
    if (t->state == ABOX_MQTT_TRIAL_RESTORING) {
        if (event == ABOX_MQTT_TRIAL_RESTORED) t->state = ABOX_MQTT_TRIAL_FAILED;
        else if (event == ABOX_MQTT_TRIAL_ERROR) t->state = ABOX_MQTT_TRIAL_RESTORE_FAILED;
        return;
    }
    if (!running(t)) return;
    if (event == ABOX_MQTT_TRIAL_ERROR) { restore(t, now, ABOX_MQTT_TRIAL_REASON_ERROR); return; }
    if (t->state == ABOX_MQTT_TRIAL_WAIT_ACCEPT && event == ABOX_MQTT_TRIAL_ACCEPTED) {
        t->started = now;
        t->state = ABOX_MQTT_TRIAL_PREPARING; action = t->port.prepare;
    } else if (t->state == ABOX_MQTT_TRIAL_PREPARING && event == ABOX_MQTT_TRIAL_PREPARED) {
        t->state = ABOX_MQTT_TRIAL_CONNECTING; action = t->port.connect;
    } else if (t->state == ABOX_MQTT_TRIAL_CONNECTING && event == ABOX_MQTT_TRIAL_CONNECTED) {
        t->state = ABOX_MQTT_TRIAL_VERIFYING; action = t->port.verify;
    } else if (t->state == ABOX_MQTT_TRIAL_VERIFYING && event == ABOX_MQTT_TRIAL_PROVED && proof == t->proof) {
        t->state = ABOX_MQTT_TRIAL_COMMITTING; action = t->port.commit;
    } else if (t->state == ABOX_MQTT_TRIAL_COMMITTING && event == ABOX_MQTT_TRIAL_SAVED) {
        t->active = t->candidate; t->state = ABOX_MQTT_TRIAL_COMMITTED;
    }
    if (action && !action(t->port.user, t->session, t->candidate)) restore(t, now, ABOX_MQTT_TRIAL_REASON_ERROR);
}
void ABoxMqttTrial_Cancel(ABoxMqttTrial *t, uint32_t now)
{
    if (!t || !running(t)) return;
    if (t->state == ABOX_MQTT_TRIAL_WAIT_ACCEPT) {
        t->reason = ABOX_MQTT_TRIAL_REASON_CANCELLED; t->state = ABOX_MQTT_TRIAL_FAILED;
    } else restore(t, now, ABOX_MQTT_TRIAL_REASON_CANCELLED);
}
int ABoxMqttTrial_RecoveryComplete(ABoxMqttTrial *t)
{
    if (!t || t->state != ABOX_MQTT_TRIAL_RESTORE_FAILED) return 0;
    t->state = ABOX_MQTT_TRIAL_FAILED; return 1;
}
