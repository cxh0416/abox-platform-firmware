#include "abox_maintenance_service.h"
int ABoxMaintenanceService_Poll(ABoxMqttTrial *trial,
    ABoxMqttTrialExecutor *executor, const ABoxMaintenanceServicePort *port,
    uint32_t now)
{
    ABoxMqttTrialExecutorAction action;
    uint64_t session;
    const ABoxMqttConfig *config;
    if (!trial || !executor || executor->trial != trial || !port || !port->action) return 0;
    ABoxMqttTrial_Poll(trial, now);
    if (!ABoxMqttTrialExecutor_Take(executor, &action, &session, &config)) return 0;
    if (!port->action(port->context, action, session, config, now))
        ABoxMqttTrial_Event(trial, session, ABOX_MQTT_TRIAL_ERROR, 0U, now);
    return 1;
}
ABoxConfigStoreResult ABoxMaintenanceService_Commit(ABoxMqttTrial *trial,
    uint64_t session, const ABoxConfigStorePort *store,
    const void *candidate, const void *stable, uint32_t now)
{
    ABoxConfigStoreResult result;
    if (!trial || trial->session != session || trial->state != ABOX_MQTT_TRIAL_COMMITTING)
        return ABOX_CONFIG_STORE_RECOVERY_FAILED;
    /* Expiry may enter RESTORING and must prevent any candidate Flash write. */
    ABoxMqttTrial_Poll(trial, now);
    if (trial->state != ABOX_MQTT_TRIAL_COMMITTING) return ABOX_CONFIG_STORE_RECOVERED;
    result = ABoxConfigStore_Commit(store, candidate, stable);
    ABoxMqttTrial_Event(trial, session,
        result == ABOX_CONFIG_STORE_SAVED ? ABOX_MQTT_TRIAL_SAVED : ABOX_MQTT_TRIAL_ERROR,
        0U, now);
    return result;
}
int ABoxMaintenanceService_Busy(const ABoxMqttTrial *trial)
{
    return trial && trial->state != ABOX_MQTT_TRIAL_IDLE &&
        trial->state != ABOX_MQTT_TRIAL_COMMITTED && trial->state != ABOX_MQTT_TRIAL_FAILED;
}
