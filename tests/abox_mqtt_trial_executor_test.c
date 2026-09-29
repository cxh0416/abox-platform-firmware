#include "abox_mqtt_trial_executor.h"

#include <assert.h>

static void take(ABoxMqttTrialExecutor *executor,
                 ABoxMqttTrialExecutorAction expected,
                 uint64_t expected_session,
                 const ABoxMqttConfig *expected_config)
{
    ABoxMqttTrialExecutorAction action = ABOX_MQTT_TRIAL_ACTION_NONE;
    const ABoxMqttConfig *config = 0;
    uint64_t session = 0U;
    assert(ABoxMqttTrialExecutor_Take(executor, &action, &session, &config));
    assert(action == expected && session == expected_session &&
           config == expected_config);
    assert(!ABoxMqttTrialExecutor_HasPending(executor));
}

int main(void)
{
    ABoxMqttTrialExecutor executor;
    ABoxMqttTrial trial;
    ABoxMqttConfig candidate = {"candidate", 1883U, "user", "secret", 0U, 0U};
    ABoxMqttConfig active = {"active", 1883U, "user", "secret", 0U, 0U};
    uint64_t session;

    assert(ABoxMqttTrialExecutor_Init(&executor, &trial));
    assert(ABoxMqttTrial_StartFirst(&trial, &candidate, 7U, 100U));
    session = trial.session;
    take(&executor, ABOX_MQTT_TRIAL_ACTION_PREPARE, session, &candidate);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_PREPARED, 0U, 101U);
    take(&executor, ABOX_MQTT_TRIAL_ACTION_CONNECT, session, &candidate);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_CONNECTED, 0U, 102U);
    take(&executor, ABOX_MQTT_TRIAL_ACTION_SUBSCRIBE, session, &candidate);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_SUBSCRIBED, 0U, 103U);
    take(&executor, ABOX_MQTT_TRIAL_ACTION_VERIFY, session, &candidate);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_PROVED, 8U, 104U);
    assert(!ABoxMqttTrialExecutor_HasPending(&executor));
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_PROVED, 7U, 105U);
    take(&executor, ABOX_MQTT_TRIAL_ACTION_COMMIT, session, &candidate);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_SAVED, 0U, 106U);
    assert(trial.state == ABOX_MQTT_TRIAL_COMMITTED);

    assert(ABoxMqttTrial_Start(&trial, &active, &candidate, 9U, 200U, 100U, 100U));
    session = trial.session;
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_ACCEPTED, 0U, 201U);
    /* A timeout replaces the queued prepare action with cleanup. */
    ABoxMqttTrial_Poll(&trial, 302U);
    assert(trial.state == ABOX_MQTT_TRIAL_RESTORING);
    take(&executor, ABOX_MQTT_TRIAL_ACTION_RESTORE, session, &active);
    ABoxMqttTrial_Event(&trial, session, ABOX_MQTT_TRIAL_RESTORED, 0U, 303U);
    assert(trial.state == ABOX_MQTT_TRIAL_FAILED);
    assert(ABoxMqttTrial_Start(&trial, &active, &candidate, 10U, 400U, 100U, 100U));
    assert(!ABoxMqttTrialExecutor_HasPending(&executor));
    return 0;
}
