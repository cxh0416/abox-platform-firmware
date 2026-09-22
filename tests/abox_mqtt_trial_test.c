#include "abox_mqtt_trial.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
typedef struct { unsigned actions, restores; int accept, restore_accept; const void *restored; } MockTrial;
static int action(void *user, uint64_t session, const void *config)
{ MockTrial *m = user; CHECK(session && config); ++m->actions; return m->accept; }
static int restore(void *user, uint64_t session, const void *config)
{ MockTrial *m = user; CHECK(session); ++m->restores; m->restored = config; return m->restore_accept; }
static void verify(ABoxMqttTrial *t)
{
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_ACCEPTED, 0, 1);
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_PREPARED, 0, 2);
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_CONNECTED, 0, 3);
    CHECK(t->state == ABOX_MQTT_TRIAL_VERIFYING);
}
int main(void)
{
    MockTrial mock = {0, 0, 1, 1, NULL};
    ABoxMqttTrialPort p = {&mock, action, action, action, action, restore};
    ABoxMqttTrial t;
    int active = 1, candidate = 2;
    uint64_t stale;
    CHECK(ABoxMqttTrial_Init(&t, &p));
    CHECK(ABoxMqttTrial_Start(&t, &active, &candidate, 42, 0, 100, 20));
    CHECK(!mock.actions); /* Must wait for old-connection accept response. */
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 42, 0);
    CHECK(t.state == ABOX_MQTT_TRIAL_WAIT_ACCEPT);
    verify(&t);
    ABoxMqttTrial_Event(&t, t.session + 1, ABOX_MQTT_TRIAL_PROVED, 42, 4);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 41, 4);
    CHECK(t.state == ABOX_MQTT_TRIAL_VERIFYING && t.active == &active);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 42, 4);
    CHECK(t.state == ABOX_MQTT_TRIAL_COMMITTING && t.active == &active);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_SAVED, 0, 5);
    CHECK(t.state == ABOX_MQTT_TRIAL_COMMITTED && t.active == &candidate);
    stale = t.session;
    CHECK(ABoxMqttTrial_Start(&t, &active, &candidate, 43, 0, 100, 20));
    ABoxMqttTrial_Event(&t, stale, ABOX_MQTT_TRIAL_ACCEPTED, 0, 1);
    CHECK(t.state == ABOX_MQTT_TRIAL_WAIT_ACCEPT);
    verify(&t); mock.accept = 0;
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 43, 4);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING && t.active == &active && mock.restored == &active);
    CHECK(!ABoxMqttTrial_Start(&t, &active, &candidate, 43, 0, 100, 20));
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_RESTORED, 0, 5);
    CHECK(t.state == ABOX_MQTT_TRIAL_FAILED);
    mock.accept = 1;
    CHECK(ABoxMqttTrial_Start(&t, &active, &candidate, 44, UINT32_MAX - 10, 20, 20));
    ABoxMqttTrial_Poll(&t, 9);
    CHECK(t.state == ABOX_MQTT_TRIAL_WAIT_ACCEPT); /* Acceptance wait is not part of the trial window. */
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_ACCEPTED, 0, UINT32_MAX - 10);
    ABoxMqttTrial_Poll(&t, 9);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING && t.reason == ABOX_MQTT_TRIAL_REASON_TIMEOUT);
    ABoxMqttTrial_Poll(&t, 29); CHECK(t.state == ABOX_MQTT_TRIAL_RESTORE_FAILED);
    CHECK(!ABoxMqttTrial_Start(&t, &active, &candidate, 43, 0, 100, 20));
    CHECK(ABoxMqttTrial_RecoveryComplete(&t));
    CHECK(ABoxMqttTrial_Start(&t, &active, &candidate, 45, 0, 10, 20));
    verify(&t);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 45, 11);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING); /* Deadline beats late proof. */
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_SAVED, 0, 11);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING && t.active == &active);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_RESTORED, 0, 12);
    CHECK(ABoxMqttTrial_Start(&t, &active, &candidate, 46, 0, 100, 20));
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_ACCEPTED, 0, 1);
    mock.restore_accept = 0; ABoxMqttTrial_Cancel(&t, 2);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORE_FAILED && t.reason == ABOX_MQTT_TRIAL_REASON_CANCELLED);
    puts("MQTT trial: acceptance gate, exact proof/session, persistence gate, rollback, deadlines and restore failure passed");
    return 0;
}
