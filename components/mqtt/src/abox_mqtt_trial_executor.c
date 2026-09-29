#include "abox_mqtt_trial_executor.h"

#include <string.h>

static int enqueue(void *user, uint64_t session, const ABoxMqttConfig *config,
                   ABoxMqttTrialExecutorAction action)
{
    ABoxMqttTrialExecutor *executor = user;
    if (!executor || !session || (action != ABOX_MQTT_TRIAL_ACTION_RESTORE && !config) ||
        (executor->pending != ABOX_MQTT_TRIAL_ACTION_NONE &&
         action != ABOX_MQTT_TRIAL_ACTION_RESTORE)) return 0;
    /* Cleanup preempts a queued action after timeout or cancellation. */
    executor->pending = action;
    executor->session = session;
    executor->config = config;
    return 1;
}

static int prepare(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_PREPARE); }
static int connect(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_CONNECT); }
static int subscribe(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_SUBSCRIBE); }
static int verify(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_VERIFY); }
static int commit(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_COMMIT); }
static int restore(void *u, uint64_t s, const ABoxMqttConfig *c)
{ return enqueue(u, s, c, ABOX_MQTT_TRIAL_ACTION_RESTORE); }

int ABoxMqttTrialExecutor_Init(ABoxMqttTrialExecutor *executor,
                               ABoxMqttTrial *trial)
{
    ABoxMqttTrialPort port;
    if (!executor || !trial) return 0;
    memset(executor, 0, sizeof(*executor));
    executor->trial = trial;
    port.user = executor;
    port.prepare = prepare;
    port.connect = connect;
    port.subscribe = subscribe;
    port.verify = verify;
    port.commit = commit;
    port.restore = restore;
    return ABoxMqttTrial_Init(trial, &port);
}

int ABoxMqttTrialExecutor_HasPending(const ABoxMqttTrialExecutor *executor)
{
    return executor && executor->pending != ABOX_MQTT_TRIAL_ACTION_NONE &&
           executor->trial && executor->session == executor->trial->session;
}

int ABoxMqttTrialExecutor_Take(ABoxMqttTrialExecutor *executor,
                               ABoxMqttTrialExecutorAction *action,
                               uint64_t *session,
                               const ABoxMqttConfig **config)
{
    int valid;
    if (!executor || !action || !session || !config) return 0;
    valid = ABoxMqttTrialExecutor_HasPending(executor);
    if (valid) {
        *action = executor->pending;
        *session = executor->session;
        *config = executor->config;
    }
    executor->pending = ABOX_MQTT_TRIAL_ACTION_NONE;
    executor->session = 0U;
    executor->config = NULL;
    return valid;
}
