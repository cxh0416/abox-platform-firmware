#include "abox_mqtt_runtime.h"
#include <stdio.h>
#include <string.h>
static uint8_t copy_text(char *d, uint16_t c, const char *s) {
  size_t n;
  if (!d || !c || !s)
    return 0;
  n = strlen(s);
  if (n >= c)
    return 0;
  memcpy(d, s, n + 1);
  return 1;
}
static void state(ABoxMqttRuntime *r, ABoxMqttRuntimeState s, const char *x) {
  r->state = s;
  if (r->callbacks.state_changed)
    r->callbacks.state_changed(r->callbacks.user, s, x);
}
static void gate(ABoxMqttRuntime *r, uint8_t on) {
  r->callbacks.set_security_ready(r->callbacks.user, on);
  r->callbacks.set_paused(r->callbacks.user, on ? 0U : 1U);
}
static uint8_t config(ABoxMqttRuntime *r, const ABoxMqttConfig *c) {
  if (!c || !c->port || c->tls_enabled > 1U ||
      (c->tls_enabled && c->tls_profile_id != r->options.tls_profile_id) ||
      !copy_text(r->host, sizeof(r->host), c->host) ||
      !copy_text(r->username, sizeof(r->username), c->username) ||
      !copy_text(r->password, sizeof(r->password), c->password))
    return 0;
  r->requested = *c;
  r->requested.host = r->host;
  r->requested.username = r->username;
  r->requested.password = r->password;
  return 1;
}
static void commit_active(ABoxMqttRuntime *r) {
  r->active = r->requested;
  (void)copy_text(r->active_host, sizeof(r->active_host), r->requested.host);
  (void)copy_text(r->active_username, sizeof(r->active_username), r->requested.username);
  (void)copy_text(r->active_password, sizeof(r->active_password), r->requested.password);
  r->active.host = r->active_host;
  r->active.username = r->active_username;
  r->active.password = r->active_password;
}
static void fail(ABoxMqttRuntime *r, const char *x) {
  gate(r, 0);
  r->waiting = 0;
  if (r->state == ABOX_MQTT_RUNTIME_DISCONNECT ||
      r->state == ABOX_MQTT_RUNTIME_UNBIND ||
      r->state == ABOX_MQTT_RUNTIME_STOP_DISCONNECT ||
      r->state == ABOX_MQTT_RUNTIME_STOP_CLOSE ||
      r->state == ABOX_MQTT_RUNTIME_STOP_UNBIND ||
      r->adapter.at->quarantined) {
    r->blocked = 1U;
    state(r, ABOX_MQTT_RUNTIME_BLOCKED, x);
    return;
  }
  state(r, ABOX_MQTT_RUNTIME_FAILED, x);
}
static int start(ABoxMqttRuntime *r, uint32_t now) {
  r->started = now;
  if (!r->requested.tls_enabled) {
    if (!r->callbacks.apply_config(r->callbacks.user, &r->requested))
      return 0;
    r->active_tls = 0;
    gate(r, 1);
    state(r, ABOX_MQTT_RUNTIME_PREPARED, "plain-prepared");
    return 1;
  }
  state(r, ABOX_MQTT_RUNTIME_WAIT_CA, "wait-ca");
  return 1;
}
static void event(ABoxEc800Event e, const uint8_t *d, uint16_t n, void *u) {
  ABoxMqttRuntime *r = u;
  char l[48];
  if (!r || e != ABOX_EC800_EVENT_LINE)
    return;
  if (n >= sizeof(l))
    n = sizeof(l) - 1;
  memcpy(l, d, n);
  l[n] = 0;
  if (!strcmp(l, "RDY")) {
    ABoxMqttRuntime_OnModemReset(r, 0);
    return;
  }
  if (r->callbacks.observe_modem_line)
    r->callbacks.observe_modem_line(r->callbacks.user, l);
}
int ABoxMqttRuntime_Init(ABoxMqttRuntime *r, ABoxEc800At *at,
                         const ABoxMqttRuntimePort *p,
                         const ABoxMqttRuntimeOptions *o,
                         const ABoxMqttConfig *c, uint32_t now) {
  ABoxEc800CommandPort q;
  if (!r || !at || !p || !o || !p->set_paused || !p->set_security_ready ||
      !p->apply_config || !p->mqtt_task || !p->mqtt_ready || !p->mqtt_connected || !p->ca_ready ||
      !p->time_valid || !o->ca_file || o->plain_client == o->tls_client ||
      o->tls_context >= ABOX_TLS_CONTEXT_CAPACITY || !o->command_timeout_ms ||
      !o->tls_timeout_ms || !o->connect_timeout_ms)
    return 0;
  memset(r, 0, sizeof(*r));
  memset(&q, 0, sizeof(q));
  r->callbacks = *p;
  r->options = *o;
  if ((c && !config(r, c)) ||
      !ABoxEc800CommandAdapter_Init(&r->adapter, at, o->owner, &q) ||
      !ABoxEc800Tls_Init(&r->tls, &q, o->supported_tls_context_mask) ||
      !ABoxMqttTls_Init(&r->mqtt_tls, &r->tls, &q, o->tls_client) ||
      !ABoxEc800At_Register(at, o->owner, event, r))
    return 0;
  r->command = q;
  r->initialized = 1;
  gate(r, 0);
  if (!c) {
    state(r, ABOX_MQTT_RUNTIME_FAILED, "await-enrollment");
    return 1;
  }
  return start(r, now);
}
int ABoxMqttRuntime_Stage(ABoxMqttRuntime *r, const ABoxMqttConfig *c) {
  if (!r || !r->initialized ||
      r->blocked || (r->state != ABOX_MQTT_RUNTIME_READY &&
       r->state != ABOX_MQTT_RUNTIME_UNENROLLED && r->state != ABOX_MQTT_RUNTIME_FAILED &&
       r->state != ABOX_MQTT_RUNTIME_PREPARED) ||
      !config(r, c))
    return 0;
  r->candidate_staged = 1;
  return 1;
}
int ABoxMqttRuntime_Activate(ABoxMqttRuntime *r, uint32_t now) {
  char c[32];
  if (!r || !r->initialized ||
      r->blocked || (r->state != ABOX_MQTT_RUNTIME_READY &&
       r->state != ABOX_MQTT_RUNTIME_UNENROLLED && r->state != ABOX_MQTT_RUNTIME_FAILED &&
       r->state != ABOX_MQTT_RUNTIME_PREPARED) ||
      !r->candidate_staged)
    return 0;
  r->candidate_staged = 0;
  r->recovering = (uint8_t)(r->state != ABOX_MQTT_RUNTIME_READY);
  if (r->callbacks.before_network_change)
    r->callbacks.before_network_change(r->callbacks.user);
  gate(r, 0);
  r->disconnect_step = 0;
  r->waiting = 0;
  r->started = now;
  state(r, ABOX_MQTT_RUNTIME_DISCONNECT, "disconnect");
  snprintf(c, sizeof(c), "AT+QMTDISC=%u",
           r->active_tls ? r->options.tls_client : r->options.plain_client);
  r->waiting = (uint8_t)r->command.submit(
      r->command.context, c, r->options.command_timeout_ms, &r->operation);
  return 1;
}
int ABoxMqttRuntime_Apply(ABoxMqttRuntime *r, const ABoxMqttConfig *c,
                          uint32_t now) {
  return ABoxMqttRuntime_Stage(r, c) && ABoxMqttRuntime_Activate(r, now);
}
int ABoxMqttRuntime_Restore(ABoxMqttRuntime *r, const ABoxMqttConfig *c,
                           uint32_t now) {
  if (!r || !r->initialized) return 0;
  if (r->state == ABOX_MQTT_RUNTIME_PREPARED ||
      r->state == ABOX_MQTT_RUNTIME_CONNECT ||
      r->state == ABOX_MQTT_RUNTIME_CONNECTED) {
    gate(r, 0);
    r->state = ABOX_MQTT_RUNTIME_FAILED;
  }
  return ABoxMqttRuntime_Apply(r, c, now);
}
static void disconnect(ABoxMqttRuntime *r, uint32_t now) {
  char c[32];
  ABoxAsyncStatus s;
  uint8_t client =
      r->active_tls ? r->options.tls_client : r->options.plain_client;
  if (!r->waiting) {
    snprintf(c, sizeof(c),
             r->disconnect_step ? "AT+QMTCLOSE=%u" : "AT+QMTDISC=%u", client);
    r->waiting = (uint8_t)r->command.submit(
        r->command.context, c, r->options.command_timeout_ms, &r->operation);
    return;
  }
  s = r->command.poll(r->command.context, r->operation);
  if (s == ABOX_ASYNC_PENDING)
    return;
  if (s != ABOX_ASYNC_OK && !r->recovering) {
    fail(r, "disconnect");
    return;
  }
  r->waiting = 0;
  if (r->disconnect_step++ == 0)
    return;
  if (r->active_tls) {
    if (!ABoxMqttTls_Unbind(&r->mqtt_tls, now, r->options.command_timeout_ms)) {
      fail(r, "unbind-start");
      return;
    }
    state(r, ABOX_MQTT_RUNTIME_UNBIND, "unbind");
  } else if (!start(r, now))
    fail(r, "plain-start");
}
static void stop_complete(ABoxMqttRuntime *r)
{
  memset(&r->active, 0, sizeof(r->active));
  memset(&r->requested, 0, sizeof(r->requested));
  r->active_host[0] = r->host[0] = '\0';
  r->candidate_staged = r->recovering = r->active_tls = 0U;
  r->waiting = 0U;
  state(r, ABOX_MQTT_RUNTIME_UNENROLLED, "stopped-first");
}
int ABoxMqttRuntime_StopFirst(ABoxMqttRuntime *r, uint32_t now)
{
  if (!r || !r->initialized) return -1;
  if (r->state == ABOX_MQTT_RUNTIME_UNENROLLED) return 1;
  if (r->blocked || r->state == ABOX_MQTT_RUNTIME_BLOCKED) return -1;
  if (r->state >= ABOX_MQTT_RUNTIME_STOP_DISCONNECT &&
      r->state <= ABOX_MQTT_RUNTIME_STOP_UNBIND) return 0;
  if (r->state != ABOX_MQTT_RUNTIME_READY &&
      r->state != ABOX_MQTT_RUNTIME_FAILED &&
      r->state != ABOX_MQTT_RUNTIME_PREPARED) return 0;
  if (r->lease.generation && !r->active_tls) {
    ABoxMqttRuntime_Block(r);
    return -1;
  }
  if (ABoxEc800At_IsBusy(r->adapter.at) ||
      r->adapter.result == ABOX_ASYNC_PENDING) return 0;
  if (r->callbacks.before_network_change)
    r->callbacks.before_network_change(r->callbacks.user);
  gate(r, 0);
  r->waiting = 0U;
  r->started = now;
  state(r, ABOX_MQTT_RUNTIME_STOP_DISCONNECT, "stop-disconnect");
  return 0;
}
void ABoxMqttRuntime_Block(ABoxMqttRuntime *r)
{
  if (!r || !r->initialized) return;
  r->blocked = 1U;
  gate(r, 0);
  state(r, ABOX_MQTT_RUNTIME_BLOCKED, "recovery-required");
}
int ABoxMqttRuntime_RecoveryComplete(ABoxMqttRuntime *r)
{
  if (!r || !r->initialized || !r->blocked ||
      r->adapter.at->quarantined || ABoxEc800At_IsBusy(r->adapter.at) ||
      r->lease.generation) return 0;
  r->blocked = 0U;
  state(r, r->active.host ? ABOX_MQTT_RUNTIME_FAILED :
           ABOX_MQTT_RUNTIME_UNENROLLED, "recovery-complete");
  return 1;
}
static void stop_poll(ABoxMqttRuntime *r, uint32_t now)
{
  char command[32];
  ABoxAsyncStatus result;
  uint8_t client = r->active_tls ? r->options.tls_client : r->options.plain_client;
  if (r->state == ABOX_MQTT_RUNTIME_STOP_UNBIND) {
    ABoxMqttTls_Poll(&r->mqtt_tls, now);
    result = r->mqtt_tls.status;
    if (result == ABOX_ASYNC_PENDING) return;
    if (result != ABOX_ASYNC_OK || !ABoxEc800Tls_Release(&r->tls, r->lease)) {
      fail(r, "stop-unbind");
      return;
    }
    memset(&r->lease, 0, sizeof(r->lease));
    stop_complete(r);
    return;
  }
  if (!r->waiting) {
    snprintf(command, sizeof(command),
             r->state == ABOX_MQTT_RUNTIME_STOP_DISCONNECT ?
             "AT+QMTDISC=%u" : "AT+QMTCLOSE=%u", client);
    r->waiting = (uint8_t)r->command.submit(r->command.context, command,
                    r->options.command_timeout_ms, &r->operation);
    return;
  }
  result = r->command.poll(r->command.context, r->operation);
  if (result == ABOX_ASYNC_PENDING) return;
  r->waiting = 0U;
  if (result == ABOX_ASYNC_QUARANTINED || result == ABOX_ASYNC_TIMEOUT ||
      (r->state == ABOX_MQTT_RUNTIME_STOP_CLOSE && result != ABOX_ASYNC_OK)) {
    fail(r, "stop-close");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_STOP_DISCONNECT) {
    state(r, ABOX_MQTT_RUNTIME_STOP_CLOSE, "stop-close");
    return;
  }
  if (!r->active_tls) { stop_complete(r); return; }
  if (!ABoxMqttTls_Unbind(&r->mqtt_tls, now, r->options.command_timeout_ms)) {
    fail(r, "stop-unbind-start");
    return;
  }
  state(r, ABOX_MQTT_RUNTIME_STOP_UNBIND, "stop-unbind");
}
void ABoxMqttRuntime_Poll(ABoxMqttRuntime *r, uint32_t now) {
  ABoxAsyncStatus s;
  if (!r || !r->initialized || r->state == ABOX_MQTT_RUNTIME_READY ||
      r->state == ABOX_MQTT_RUNTIME_FAILED ||
      r->state == ABOX_MQTT_RUNTIME_UNENROLLED ||
      r->state == ABOX_MQTT_RUNTIME_BLOCKED)
    return;
  if (r->state >= ABOX_MQTT_RUNTIME_STOP_DISCONNECT &&
      r->state <= ABOX_MQTT_RUNTIME_STOP_UNBIND) {
    stop_poll(r, now);
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_DISCONNECT) {
    disconnect(r, now);
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_UNBIND) {
    ABoxMqttTls_Poll(&r->mqtt_tls, now);
    s = r->mqtt_tls.status;
    if (s == ABOX_ASYNC_PENDING)
      return;
    if (s != ABOX_ASYNC_OK || !ABoxEc800Tls_Release(&r->tls, r->lease)) {
      fail(r, "unbind");
      return;
    }
    memset(&r->lease, 0, sizeof(r->lease));
    r->active_tls = 0;
    if (!start(r, now))
      fail(r, "post-unbind");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_CONNECT) {
    r->callbacks.mqtt_task(r->callbacks.user);
    if (r->callbacks.mqtt_connected(r->callbacks.user)) {
      r->started = now;
      state(r, ABOX_MQTT_RUNTIME_CONNECTED, "connected");
    } else if ((uint32_t)(now - r->started) >= r->options.connect_timeout_ms)
      fail(r, "connect");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_CONNECTED) {
    r->callbacks.mqtt_task(r->callbacks.user);
    if (r->callbacks.mqtt_ready(r->callbacks.user)) {
      commit_active(r);
      state(r, ABOX_MQTT_RUNTIME_READY, "ready");
    } else if ((uint32_t)(now - r->started) >=
               (r->options.subscribe_timeout_ms ? r->options.subscribe_timeout_ms
                                                : r->options.connect_timeout_ms))
      fail(r, "subscribe");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_PREPARED) {
    r->started = now;
    state(r, ABOX_MQTT_RUNTIME_CONNECT, "connect");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_WAIT_CA) {
    if (!r->callbacks.ca_ready(r->callbacks.user))
      return;
    if (!r->command.submit(r->command.context, "AT+CCLK?",
                           r->options.command_timeout_ms, &r->operation))
      return;
    state(r, ABOX_MQTT_RUNTIME_CLOCK, "clock");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_CLOCK) {
    ABoxTlsProfile p;
    s = r->command.poll(r->command.context, r->operation);
    if (s == ABOX_ASYNC_PENDING)
      return;
    if (s != ABOX_ASYNC_OK || !r->callbacks.time_valid(r->callbacks.user)) {
      fail(r, "clock");
      return;
    }
    p.ca_file = r->options.ca_file;
    p.ca_revision = r->options.ca_revision;
    p.ca_verified = 1;
    p.time_valid = 1;
    if (!ABoxEc800Tls_Acquire(&r->tls, r->options.tls_context, r->options.owner,
                              &r->lease) ||
        !ABoxEc800Tls_Prepare(&r->tls, r->lease, &p, now,
                              r->options.tls_timeout_ms)) {
      fail(r, "prepare-start");
      return;
    }
    state(r, ABOX_MQTT_RUNTIME_PREPARE, "prepare");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_PREPARE) {
    ABoxEc800Tls_Poll(&r->tls, now);
    s = ABoxEc800Tls_Status(&r->tls, r->lease);
    if (s == ABOX_ASYNC_PENDING)
      return;
    if (s != ABOX_ASYNC_OK ||
        !ABoxMqttTls_Start(&r->mqtt_tls, 1, r->lease, now,
                           r->options.command_timeout_ms)) {
      fail(r, "prepare");
      return;
    }
    state(r, ABOX_MQTT_RUNTIME_BIND, "bind");
    return;
  }
  if (r->state == ABOX_MQTT_RUNTIME_BIND) {
    ABoxMqttTls_Poll(&r->mqtt_tls, now);
    s = r->mqtt_tls.status;
    if (s == ABOX_ASYNC_PENDING)
      return;
    if (s != ABOX_ASYNC_OK ||
        !r->callbacks.apply_config(r->callbacks.user, &r->requested)) {
      fail(r, "bind");
      return;
    }
    r->active_tls = 1;
    r->started = now;
    gate(r, 1);
    state(r, ABOX_MQTT_RUNTIME_PREPARED, "tls-prepared");
  }
}
void ABoxMqttRuntime_OnModemReset(ABoxMqttRuntime *r, uint32_t now) {
  if (!r || !r->initialized)
    return;
  ABoxEc800CommandAdapter_OnModemReset(&r->adapter);
  ABoxEc800Tls_OnModemReset(&r->tls);
  ABoxMqttTls_OnModemReset(&r->mqtt_tls);
  memset(&r->lease, 0, sizeof(r->lease));
  r->active_tls = 0;
  gate(r, 0);
  if (r->blocked) {
    state(r, ABOX_MQTT_RUNTIME_BLOCKED, "recovery-required");
    return;
  }
  if (!r->requested.host) {
    state(r, ABOX_MQTT_RUNTIME_FAILED, "await-enrollment");
    return;
  }
  if (!start(r, now))
    fail(r, "modem-reset");
}
uint8_t ABoxMqttRuntime_IsReady(const ABoxMqttRuntime *r) {
  return r && r->state == ABOX_MQTT_RUNTIME_READY;
}
ABoxMqttRuntimeState ABoxMqttRuntime_GetState(const ABoxMqttRuntime *r) {
  return r ? r->state : ABOX_MQTT_RUNTIME_BLOCKED;
}
uint8_t ABoxMqttRuntime_IsTlsActive(const ABoxMqttRuntime *r) {
  return r && r->active_tls;
}
uint8_t ABoxMqttRuntime_ActiveClient(const ABoxMqttRuntime *r) {
  return r && r->active_tls ? r->options.tls_client
                            : (r ? r->options.plain_client : 0);
}
uint8_t ABoxMqttRuntime_ActiveContext(const ABoxMqttRuntime *r) {
  return r && r->active_tls ? r->options.tls_context : 0;
}
const ABoxMqttConfig *ABoxMqttRuntime_ActiveConfig(const ABoxMqttRuntime *r) {
  return r && r->active.host ? &r->active : 0;
}
const ABoxMqttConfig *ABoxMqttRuntime_StagedConfig(const ABoxMqttRuntime *r) {
  return r && r->candidate_staged ? &r->requested : 0;
}
