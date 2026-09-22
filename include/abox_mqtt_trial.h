#ifndef ABOX_MQTT_TRIAL_H
#define ABOX_MQTT_TRIAL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    ABOX_MQTT_TRIAL_IDLE, ABOX_MQTT_TRIAL_WAIT_ACCEPT,
    ABOX_MQTT_TRIAL_PREPARING, ABOX_MQTT_TRIAL_CONNECTING,
    ABOX_MQTT_TRIAL_VERIFYING, ABOX_MQTT_TRIAL_COMMITTING,
    ABOX_MQTT_TRIAL_RESTORING, ABOX_MQTT_TRIAL_COMMITTED,
    ABOX_MQTT_TRIAL_FAILED, ABOX_MQTT_TRIAL_RESTORE_FAILED
} ABoxMqttTrialState;
typedef enum {
    ABOX_MQTT_TRIAL_ACCEPTED, ABOX_MQTT_TRIAL_PREPARED,
    ABOX_MQTT_TRIAL_CONNECTED, ABOX_MQTT_TRIAL_PROVED,
    ABOX_MQTT_TRIAL_SAVED, ABOX_MQTT_TRIAL_RESTORED,
    ABOX_MQTT_TRIAL_ERROR
} ABoxMqttTrialEvent;
typedef enum {
    ABOX_MQTT_TRIAL_REASON_NONE, ABOX_MQTT_TRIAL_REASON_ERROR,
    ABOX_MQTT_TRIAL_REASON_TIMEOUT, ABOX_MQTT_TRIAL_REASON_CANCELLED
} ABoxMqttTrialReason;
/* Immutable active/candidate objects belong to caller, including layout and
 * credentials. Keep alive through restore/commit. Callbacks only enqueue work;
 * return 0 on refusal, 1 on acceptance, never recursively call Event/Poll.
 * SAVED attests persistence + readback. RESTORED attests cleanup and reconnection.
 * prepare/restore must quiesce outstanding work from the previous phase. */
typedef int (*ABoxMqttTrialAction)(void *user, uint64_t session, const void *config);
typedef struct {
    void *user;
    ABoxMqttTrialAction prepare, connect, verify, commit, restore;
} ABoxMqttTrialPort;
typedef struct {
    ABoxMqttTrialPort port;
    const void *active, *candidate;
    uint64_t session, serial, proof;
    uint32_t started, timeout, restore_started, restore_timeout;
    ABoxMqttTrialState state;
    ABoxMqttTrialReason reason;
} ABoxMqttTrial;
int ABoxMqttTrial_Init(ABoxMqttTrial *trial, const ABoxMqttTrialPort *port);
int ABoxMqttTrial_Start(ABoxMqttTrial *trial, const void *active, const void *candidate,
                       uint64_t proof_id, uint32_t now, uint32_t timeout,
                       uint32_t restore_timeout);
/* Session correlates every event; PROVED additionally requires exact proof ID.
 * The trial timeout starts at ACCEPTED, after the old connection's response is
 * delivered. Call at event arrival time so an expired candidate cannot commit. */
void ABoxMqttTrial_Event(ABoxMqttTrial *trial, uint64_t session,
                       ABoxMqttTrialEvent event, uint64_t proof_id, uint32_t now);
void ABoxMqttTrial_Poll(ABoxMqttTrial *trial, uint32_t now);
void ABoxMqttTrial_Cancel(ABoxMqttTrial *trial, uint32_t now);
/* Restore failure blocks new trials until caller explicitly attests recovery. */
int ABoxMqttTrial_RecoveryComplete(ABoxMqttTrial *trial);
#ifdef __cplusplus
}
#endif
#endif
