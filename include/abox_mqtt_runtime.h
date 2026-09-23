#ifndef ABOX_MQTT_RUNTIME_H
#define ABOX_MQTT_RUNTIME_H
#include "abox_ec800_at.h"
#include "abox_ec800_command_port.h"
#include "abox_ec800_tls.h"
#include "abox_mqtt_tls.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ABOX_MQTT_RUNTIME_HOST_SIZE 64U
#define ABOX_MQTT_RUNTIME_USERNAME_SIZE 32U
#define ABOX_MQTT_RUNTIME_PASSWORD_SIZE 64U
typedef struct {
  const char *host;
  uint16_t port;
  const char *username;
  const char *password;
  uint8_t tls_enabled;
  uint8_t tls_profile_id;
} ABoxMqttConfig;
typedef enum {
  ABOX_MQTT_RUNTIME_IDLE = 0,
  ABOX_MQTT_RUNTIME_DISCONNECT,
  ABOX_MQTT_RUNTIME_UNBIND,
  ABOX_MQTT_RUNTIME_WAIT_CA,
  ABOX_MQTT_RUNTIME_CLOCK,
  ABOX_MQTT_RUNTIME_PREPARE,
  ABOX_MQTT_RUNTIME_BIND,
  ABOX_MQTT_RUNTIME_PREPARED,
  ABOX_MQTT_RUNTIME_CONNECT,
  ABOX_MQTT_RUNTIME_CONNECTED,
  ABOX_MQTT_RUNTIME_READY,
  ABOX_MQTT_RUNTIME_FAILED
} ABoxMqttRuntimeState;
typedef struct {
  void *user;
  void (*set_paused)(void *, uint8_t);
  void (*set_security_ready)(void *, uint8_t);
  int (*apply_config)(void *, const ABoxMqttConfig *);
  void (*mqtt_task)(void *);
  uint8_t (*mqtt_ready)(void *);
  uint8_t (*mqtt_connected)(void *);
  uint8_t (*ca_ready)(void *);
  uint8_t (*time_valid)(void *);
  void (*observe_modem_line)(void *, const char *);
  void (*before_network_change)(void *);
  void (*state_changed)(void *, ABoxMqttRuntimeState, const char *);
} ABoxMqttRuntimePort;
typedef struct {
  uint32_t owner, command_timeout_ms, tls_timeout_ms, connect_timeout_ms,
      subscribe_timeout_ms;
  const char *ca_file;
  uint32_t ca_revision;
  uint8_t plain_client, tls_client, tls_context, tls_profile_id,
      supported_tls_context_mask;
} ABoxMqttRuntimeOptions;
typedef struct {
  ABoxMqttRuntimePort callbacks;
  ABoxMqttRuntimeOptions options;
  ABoxEc800CommandAdapter adapter;
  ABoxEc800CommandPort command;
  ABoxEc800Tls tls;
  ABoxMqttTls mqtt_tls;
  ABoxTlsLease lease;
  uint64_t operation;
  ABoxMqttRuntimeState state;
  uint32_t started;
  uint8_t waiting, disconnect_step, active_tls, initialized, candidate_staged, recovering;
  ABoxMqttConfig requested, active;
  char host[64], username[32], password[64];
  char active_host[64], active_username[32], active_password[64];
} ABoxMqttRuntime;
int ABoxMqttRuntime_Init(ABoxMqttRuntime *, ABoxEc800At *,
                         const ABoxMqttRuntimePort *,
                         const ABoxMqttRuntimeOptions *, const ABoxMqttConfig *,
                         uint32_t);
void ABoxMqttRuntime_Poll(ABoxMqttRuntime *, uint32_t);
int ABoxMqttRuntime_Stage(ABoxMqttRuntime *, const ABoxMqttConfig *);
int ABoxMqttRuntime_Activate(ABoxMqttRuntime *, uint32_t);
int ABoxMqttRuntime_Apply(ABoxMqttRuntime *, const ABoxMqttConfig *, uint32_t);
int ABoxMqttRuntime_Restore(ABoxMqttRuntime *, const ABoxMqttConfig *, uint32_t);
void ABoxMqttRuntime_OnModemReset(ABoxMqttRuntime *, uint32_t);
uint8_t ABoxMqttRuntime_IsReady(const ABoxMqttRuntime *);
uint8_t ABoxMqttRuntime_ActiveClient(const ABoxMqttRuntime *);
uint8_t ABoxMqttRuntime_ActiveContext(const ABoxMqttRuntime *);
const ABoxMqttConfig *ABoxMqttRuntime_ActiveConfig(const ABoxMqttRuntime *);
const ABoxMqttConfig *ABoxMqttRuntime_StagedConfig(const ABoxMqttRuntime *);
#ifdef __cplusplus
}
#endif
#endif
