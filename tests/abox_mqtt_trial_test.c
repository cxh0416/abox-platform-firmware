#include "abox_mqtt_trial.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
typedef struct { unsigned actions, restores; int accept, restore_accept; const ABoxMqttConfig *restored; } MockTrial;
static int action(void *user, uint64_t session, const ABoxMqttConfig *config)
{ MockTrial *m = user; CHECK(session && config); ++m->actions; return m->accept; }
static int restore(void *user, uint64_t session, const ABoxMqttConfig *config)
{ MockTrial *m = user; CHECK(session); ++m->restores; m->restored = config; return m->restore_accept; }
static void verify(ABoxMqttTrial *t)
{
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_ACCEPTED, 0, 1);
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_PREPARED, 0, 2);
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_CONNECTED, 0, 3);
    CHECK(t->state == ABOX_MQTT_TRIAL_SUBSCRIBING);
    ABoxMqttTrial_Event(t, t->session, ABOX_MQTT_TRIAL_SUBSCRIBED, 0, 3);
    CHECK(t->state == ABOX_MQTT_TRIAL_VERIFYING);
}
static void check_security_transitions(const ABoxMqttTrialPort *port)
{
    ABoxMqttConfig plain_a = {"a.example", 1883U, "u", "p", 0U, 0U};
    ABoxMqttConfig plain_b = {"b.example", 1884U, "u", "p", 0U, 0U};
    ABoxMqttConfig tls_a = {"a.example", 8883U, "u", "p", 1U, 1U};
    ABoxMqttConfig tls_b = {"b.example", 8884U, "u", "p", 1U, 2U};
    const ABoxMqttConfig *from[] = {&plain_a, &plain_a, &tls_a, &tls_a};
    const ABoxMqttConfig *to[] = {&tls_a, &plain_b, &tls_b, &plain_b};
    ABoxMqttTrial trial;
    unsigned i;
    CHECK(ABoxMqttTrial_Init(&trial, port));
    for (i = 0U; i < 4U; ++i) {
        CHECK(ABoxMqttTrial_Start(&trial, from[i], to[i], 100U + i, 0U, 100U, 20U));
        CHECK(trial.active == from[i] && trial.candidate == to[i]);
        ABoxMqttTrial_Cancel(&trial, 0U);
        CHECK(trial.state == ABOX_MQTT_TRIAL_FAILED && trial.active == from[i]);
    }
}
int main(void)
{
    MockTrial mock = {0, 0, 1, 1, NULL};
    ABoxMqttTrialPort p = {&mock, action, action, action, action, action, restore};
    ABoxMqttTrial t;
    ABoxMqttConfig active = {"plain.example", 1883U, "u", "p", 0U, 0U};
    ABoxMqttConfig candidate = {"tls.example", 8883U, "u2", "p2", 1U, 3U};
    ABoxMqttConfig invalid = {"", 1883U, "u", "p", 0U, 0U};
    uint64_t stale;
    CHECK(ABoxMqttTrial_Init(&t, &p));
    check_security_transitions(&p);
    CHECK(!ABoxMqttTrial_Start(&t, &active, &invalid, 42, 0, 100, 20));
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
    CHECK(ABoxMqttTrial_RecoveryComplete(&t));
    mock.restore_accept = 1;
    CHECK(ABoxMqttTrial_StartFirst(&t, &candidate, 47, 100));
    CHECK(t.state == ABOX_MQTT_TRIAL_PREPARING && t.active == NULL);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PREPARED, 0, 101);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_CONNECTED, 0, 102);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_SUBSCRIBED, 0, 103);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_PROVED, 47, 104);
    CHECK(t.state == ABOX_MQTT_TRIAL_COMMITTING && t.active == NULL);
    ABoxMqttTrial_Event(&t, t.session, ABOX_MQTT_TRIAL_SAVED, 0, 105);
    CHECK(t.state == ABOX_MQTT_TRIAL_COMMITTED && t.active == &candidate);
    CHECK(ABoxMqttTrial_StartFirst(&t, &candidate, 48, UINT32_MAX - 10U));
    ABoxMqttTrial_Poll(&t, 119988U);
    CHECK(t.state == ABOX_MQTT_TRIAL_PREPARING);
    ABoxMqttTrial_Poll(&t, 119989U);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING && mock.restored == NULL);
    ABoxMqttTrial_Poll(&t, 209989U);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORE_FAILED);
    CHECK(!ABoxMqttTrial_StartFirst(&t, &candidate, 49, 0));
    CHECK(ABoxMqttTrial_RecoveryComplete(&t));
    CHECK(ABoxMqttTrial_StartFirst(&t, &candidate, 49, 0));
    ABoxMqttTrial_Cancel(&t, 1);
    CHECK(t.state == ABOX_MQTT_TRIAL_RESTORING && mock.restored == NULL);
    puts("MQTT trial: typed config, connect/subscribe/proof gates, persistence, rollback and deadlines passed");
    return 0;
}
