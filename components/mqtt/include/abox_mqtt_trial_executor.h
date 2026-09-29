#ifndef ABOX_MQTT_TRIAL_EXECUTOR_H
#define ABOX_MQTT_TRIAL_EXECUTOR_H

#include "abox_mqtt_trial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ABOX_MQTT_TRIAL_ACTION_NONE = 0,
    ABOX_MQTT_TRIAL_ACTION_PREPARE,
    ABOX_MQTT_TRIAL_ACTION_CONNECT,
    ABOX_MQTT_TRIAL_ACTION_SUBSCRIBE,
    ABOX_MQTT_TRIAL_ACTION_VERIFY,
    ABOX_MQTT_TRIAL_ACTION_COMMIT,
    ABOX_MQTT_TRIAL_ACTION_RESTORE
} ABoxMqttTrialExecutorAction;

typedef struct {
    ABoxMqttTrial *trial;
    ABoxMqttTrialExecutorAction pending;
    uint64_t session;
    const ABoxMqttConfig *config;
} ABoxMqttTrialExecutor;

/* Binds the trial state machine to one deferred action slot. Take actions
 * only from the communication owner task, then report their actual result
 * through ABoxMqttTrial_Event. Callbacks never perform nested AT or Flash. */
int ABoxMqttTrialExecutor_Init(ABoxMqttTrialExecutor *executor,
                               ABoxMqttTrial *trial);
int ABoxMqttTrialExecutor_HasPending(const ABoxMqttTrialExecutor *executor);
int ABoxMqttTrialExecutor_Take(ABoxMqttTrialExecutor *executor,
                               ABoxMqttTrialExecutorAction *action,
                               uint64_t *session,
                               const ABoxMqttConfig **config);

#ifdef __cplusplus
}
#endif
#endif
