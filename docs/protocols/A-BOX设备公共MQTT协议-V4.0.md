# A-BOX 设备公共 MQTT 协议

**文档修订：V4.0 正式冻结版**

**报文版本：`4.0`**

**状态：Frozen Public Contract（正式冻结）**

**修订日期：2026-09-16**

> 五 Topic、公共消息模型、`command` 控制路由、`reportType + reportName` 上报路由、Manifest 能力治理及第 50 节参数均已冻结。兼容扩展必须遵循第 47 节，改变既有公共语义必须升级公共协议主版本。

---

## 1. 协议定位

本协议定义车载 A-BOX 与业务平台之间统一、稳定的 MQTT 公共通信框架。

A-BOX 为车载集成控制器，可根据不同车型同时对接一种或多种业务能力，例如：

- 车辆底盘；
- 飞翼；
- 格口；
- 冷机；
- 称重设备；
- 其他后续扩展设备。

不同车型安装的业务能力组合可以不同，但相同业务能力必须复用相同的命令、状态和上报语义，不得因车型、客户或项目重新定义公共通信框架。

本公共协议定义：

- MQTT Topic；
- MQTT 会话策略；
- 公共 JSON 外层；
- 平台请求；
- A-BOX 响应；
- A-BOX 主动业务上报；
- A-BOX 心跳；
- Manifest 能力声明；
- 请求分类、过期与幂等；
- 状态同步；
- Report 去重、多实例与可选顺序控制；
- QoS 与 Retain；
- 公共响应码；
- Profile 版本兼容；
- 安全、连接与兼容性规则。

具体业务 Profile 负责定义：

- `command`；
- `params`；
- 可执行操作及操作类别；
- `200` 的业务成功边界；
- 业务专用响应码；
- `result`；
- `reportName`；
- `payload`；
- 多实例规则；
- 状态枚举；
- 业务安全语义。

A-BOX 内部 CAN、RS485、UART、IO、执行器拓扑、轮询机制、设备地址、安全状态机、控制仲裁及故障恢复算法均不属于公共平台协议。

设备信息、远程日志、配置管理、诊断、OTA 等内部运维能力继续使用独立 `/internal/*` 协议，不并入本公共业务协议。

---

## 2. 核心设计原则

### 2.1 A-BOX 是 MQTT 通信主体

每台物理 A-BOX 由平台分配全局唯一的：

```text
deviceId
```

`deviceId` 用于 MQTT Topic，不要求重复出现在每条 Payload 中。

公共业务请求不依赖 `deviceType`、`componentType`、`componentId` 或 `profileType` 进行路由。

---

### 2.2 `command` 负责控制路由

平台在项目对接阶段已明确车辆支持的业务能力。

平台通过：

```text
command + params
```

表达具体业务请求。

例如：

```text
wing_control
open_locker
check_locker_status
refrigeration_control
vehicle_control
```

A-BOX 根据 `command` 和对应业务参数，在执行前确定具体可执行操作并完成路由。

---

### 2.3 `reportType + reportName` 负责主动上报路由

所有非心跳类主动业务消息统一进入 `/report`。

公共层使用：

```text
reportType + reportName
```

识别消息类别和具体业务语义。

业务字段全部放入：

```text
payload
```

---

### 2.4 Profile 是能力治理概念，不是逐条消息路由层

一台 A-BOX 可以实现多个 Profile。

例如：

```text
A-BOX
├─ wing
├─ refrigeration
└─ weighing
```

Profile 用于定义命令、报告、版本和业务语义，但不要求每条请求或 Report 强制携带 Profile 标识。

---

### 2.5 多实例按需表达

如果一个 Profile 在同一 A-BOX 中仅存在单实例，不要求携带实例标识。

如果一个 Profile 支持多实例，则：

- 主动上报使用公共可选字段 `instanceId`；
- 请求侧由该 Profile 在 `params` 中定义目标实例字段；
- Profile 必须明确 `instanceId` 的格式、长度和唯一性范围。

---

## 3. 公共 MQTT Topic

V4 固定使用以下五个公共 Topic：

| 方向 | 用途 | Topic |
|---|---|---|
| 平台 → A-BOX | 业务请求 | `/zxwl/abox/{deviceId}/request` |
| A-BOX → 平台 | 请求响应 | `/zxwl/abox/{deviceId}/response` |
| A-BOX → 平台 | 主动业务上报 | `/zxwl/abox/{deviceId}/report` |
| A-BOX → 平台 | 心跳 | `/zxwl/abox/{deviceId}/heartbeat` |
| A-BOX → 平台 | 能力清单 | `/zxwl/abox/{deviceId}/manifest` |

平台可以使用通配符统一订阅：

```text
/zxwl/abox/+/response
/zxwl/abox/+/report
/zxwl/abox/+/heartbeat
/zxwl/abox/+/manifest
```

具体业务 Profile 不得修改公共 Topic 结构。

---

## 4. MQTT 会话策略

公共业务控制通道不得依赖 Broker 离线队列实现控制命令补发。

### 4.1 MQTT 3.1.1

必须使用：

```text
Clean Session = true
```

### 4.2 MQTT 5.0

A-BOX 公共业务 MQTT 客户端建立连接时必须同时使用：

```text
Clean Start = true
Session Expiry Interval = 0
```

该要求只针对 **A-BOX 公共业务 MQTT 客户端**，目的是避免公共业务控制通道恢复旧会话或消费离线期间积压的控制请求。

平台侧 MQTT 消费者、Broker Bridge、Kafka Connector 等服务端组件是否使用持久会话，由平台部署架构自行决定，不受本条强制。

### 4.3 请求 Topic 禁止 Retain

平台发布 `/request` 时：

```text
Retain = false
```

不得通过 Retained Request 表达任何控制意图。

### 4.4 可恢复运维业务

允许离线恢复的配置、诊断、日志和 OTA 等能力继续使用独立 `/internal/*` 运维协议，不与公共实时控制混用。

---

## 5. MQTT QoS 与 Retain

| 消息类型 | QoS | Retain |
|---|---:|---:|
| `/request` | 1 | 否 |
| `/response` | 1 | 否 |
| `/heartbeat` | 0 | 否 |
| `/manifest` | 1 | 是 |
| `state` Report | 1 | 否 |
| `event` Report | 1 | 否 |
| `telemetry` Report | 0 | 否 |

规则：

1. `state` 和 `event` 不得降为 QoS 0。
2. `telemetry` 固定使用 QoS 0。
3. 如果某项数据必须可靠投递，则应重新评估其语义是否实际属于 `state` 或 `event`，不得仅通过把 `telemetry` 提升到 QoS 1 规避分类。
4. 平台订阅 `/report` 时必须请求 QoS 1，以避免 `state` 和 `event` 的实际交付 QoS 被订阅端降为 0。
5. `/report` 统一不使用 Retain，状态恢复使用第 26～29 节定义的机制。

---

## 6. 公共 JSON 外层

所有 MQTT Payload：

- 使用 UTF-8；
- 使用 JSON；
- 固定 Topic 段使用小写；
- 外层固定包含 `version`、`timestamp`、`data`。

基础结构：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {}
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `version` | String | 是 | 公共协议版本，V4 固定为 `"4.0"` |
| `timestamp` | Integer | 是 | UTC Unix Epoch 毫秒 |
| `data` | Object | 是 | 消息内容 |

### 6.1 Epoch 毫秒字段

`timestamp`、`expiresAt` 以及未来新增的所有 Epoch 毫秒字段均具有 64 位整数语义。

必须以十进制整数原样输出，例如：

```json
"timestamp": 1770186088333
```

不得输出：

```text
1.770186088333e+12
```

不得因 JSON 库内部浮点转换产生截断、舍入或科学计数法。

所有公共数字字段只允许 `-9007199254740991..9007199254740991` 范围内的整数。Epoch 毫秒字段必须是非负十进制整数，不接受字符串、小数、指数形式、前导 `+` 或负零。整数边界和非法词法测试向量见 `docs/protocols/v4/test-vectors.json`。

### 6.2 重复 JSON Key

任何 JSON Object 中出现重复 Key 均视为非法报文。

例如：

```json
{
  "requestId": "A",
  "requestId": "B"
}
```

必须拒绝，不得自行选择其中某一个值。

### 6.3 未知字段

接收方必须忽略当前版本中不认识但可安全忽略的扩展字段。

不得仅因出现未知可选字段而丢弃整条合法消息。

### 6.4 未知枚举

接收方遇到未知业务枚举时：

- 必须保留原始值；
- 可以降级展示为 `unknown`；
- 不得把未知值解释为成功、完成、停止、关闭、正常或任何其他安全状态。

---

## 7. 平台请求

Topic：

```text
/zxwl/abox/{deviceId}/request
```

示例：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "command": "wing_control",
    "expiresAt": 1770186093333,
    "params": {
      "action": "ascend"
    }
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `requestId` | String | 是 | 平台生成的全局唯一请求标识 |
| `command` | String | 是 | 业务命令 |
| `expiresAt` | Integer | 按操作类别 | UTC Unix Epoch 毫秒失效时间 |
| `params` | Object / null | 是 | 业务参数，无参数时传 `null` |

---

## 8. 可执行操作分类

V4 不要求“每个 command 只能属于一个类别”。

要求是：

> 每个可执行操作必须在真正执行前唯一确定所属类别。

如果同一个 `command` 的不同参数组合属于不同类别，Profile 必须按“命令 + 操作判定条件”分别登记。

例如：

| command | 操作判定 | category |
|---|---|---|
| `wing_control` | `action=ascend` | `DISCRETE_ACTION` |
| `wing_control` | `action=descend` | `DISCRETE_ACTION` |
| `wing_control` | `action=stop` | `SAFETY_STOP` |

V4 定义以下四种操作类别：

| 类别 | 标识 | 典型用途 |
|---|---|---|
| 查询 | `QUERY` | 查询状态、读取信息、`sync_state` |
| 离散副作用动作 | `DISCRETE_ACTION` | 开门、开锁、单次升降等 |
| 连续会话控制 | `CONTINUOUS_SESSION` | 底盘连续驾驶等 |
| 安全停止 | `SAFETY_STOP` | stop、emergency stop |

---

## 9. QUERY

查询类操作：

- 不应产生不可逆物理副作用；
- 相同 `requestId`、相同请求内容重试时允许重新执行；
- 可以重新生成同步结果；
- 如果会产生 Report，则重试时生成新的 `reportId`；
- `expiresAt` 默认可选。

`sync_state` 属于 `QUERY`。

---

## 10. DISCRETE_ACTION

离散副作用动作例如：

```text
open_locker
wing_control(action=ascend)
wing_control(action=descend)
```

此类操作必须：

1. 携带 `expiresAt`；
2. 由 Profile 定义最大允许有效期；
3. 实现 `requestId` 幂等；
4. 同一请求不得因 MQTT 重投重复执行；
5. 必须处理 A-BOX 重启导致 RAM 幂等记录丢失的问题。

Profile 必须声明其跨重启保护机制：

```text
persistent_idempotency
```

或：

```text
session_constraint
```

或其他具有等价安全性的方案。

---

## 11. CONTINUOUS_SESSION

连续会话控制不得依赖持久化每条高频请求实现安全。

对应 Profile 必须定义至少具有以下等价能力的机制：

```text
sessionId
sequence
lease / timeout
```

设备必须能够在：

- 会话失效；
- 网络断开；
- 租约超时；
- 控制序号异常；

等情况下进入 Profile 规定的安全状态。

公共 `requestId` 幂等不能替代连续会话控制自身的实时安全机制。

---

## 12. SAFETY_STOP

安全停止类操作由 Profile 明确定义。

原则上：

- 应设计为天然幂等；
- 可定义不同于普通动作的过期规则；
- 可允许重复执行；
- 可以定义绕过普通业务 Busy 状态；
- 不得绕过身份认证、基本报文合法性和必要参数校验。

---

## 13. 请求过期

对于要求 `expiresAt` 的操作：

```text
currentTime >= expiresAt
```

时，该请求已经过期。

A-BOX 必须拒绝执行，并返回：

```text
4005 REQUEST_EXPIRED
```

规则：

1. `expiresAt` 表示请求最晚允许开始接受执行的时刻。
2. 动作一旦被接受后允许持续多久，由业务 Profile 的超时、租约或安全状态机决定。
3. 每个要求 `expiresAt` 的可执行操作必须定义 `maxValidity`。
   `DISCRETE_ACTION` 的 `maxValidity` 不得超过 30 秒，Profile 只能定义更短值。
4. A-BOX 接收请求时必须满足：

```text
0 < expiresAt - currentTime <= maxValidity
```

5. `expiresAt - currentTime <= 0` 时返回 `4005 REQUEST_EXPIRED`。
6. `expiresAt - currentTime > maxValidity` 时视为非法请求，返回 `4001 INVALID_REQUEST`。
7. 依赖 `expiresAt` 的设备必须具备可信时间源。
8. 设备无法确认当前时间有效时，不得静默忽略 `expiresAt`；必须按 Profile 规则拒绝时间敏感动作或进入安全降级状态。

---

## 14. 请求幂等与重试

### 14.1 请求内容相等规则

逻辑请求内容由：

```text
command
+ 规范化后的 params
+ expiresAt
```

决定。

外层：

```text
timestamp
```

不参与请求内容一致性判断。

不得基于：

- 原始 JSON 字符串；
- Object 字段顺序；
- 无意义空白；

判断两个请求是否相同。

### 14.1.1 请求摘要规范化

需要持久化或计算请求摘要时，必须对以下逻辑对象使用与 Manifest 相同基础规则的 **V4 JSON Canonicalization**：

```json
{
  "command": "...",
  "params": null,
  "expiresAt": 1770186093333
}
```

规则：

1. `command` 必须参与摘要。
2. `params` 必须参与摘要。
3. 如果该请求携带 `expiresAt`，则 `expiresAt` 必须参与摘要。
4. 如果该操作不使用 `expiresAt`，规范化逻辑对象中必须省略 `expiresAt`，不得自行补 `null`。
5. `params` 是公共请求必填字段，因此“缺少 params”属于非法请求；`params: null` 与 `params: {}` 不等价。
6. JSON Object 字段顺序不影响摘要。
7. JSON Array 顺序默认具有业务意义，必须保持原顺序；只有具体 Profile 明确声明某个数组为集合语义时，才允许在业务层先进行集合规范化。
8. 字符串比较基于解码后的 Unicode 字符值，并使用统一 UTF-8 Canonicalization。
9. V4 公共数字值只允许安全整数；小数、指数形式、负零、`NaN` 和无穷值均非法。
10. 重复 JSON Key 已由第 6.2 节禁止，不得进入 Canonicalization。

例如：

```json
{"a":1,"b":2}
```

与：

```json
{"b":2,"a":1}
```

在 Object 值语义相同的前提下必须产生相同 Canonical Bytes。

但：

```json
[1,2]
```

与：

```json
[2,1]
```

默认视为不同值。

跨嵌入式与平台实现必须使用 `docs/protocols/v4/test-vectors.json` 中一致的请求摘要测试向量：

```text
Request Logical Object
→ Canonical Bytes
→ Request Digest
```

Request Digest 固定为规范字节的 SHA-256，以 64 位小写十六进制表达；实现方不得选择其他算法。

### 14.2 requestId 冲突

如果相同 `requestId` 对应不同逻辑请求内容：

```text
command / params / expiresAt
```

任一不同，则返回：

```text
4006 REQUEST_ID_CONFLICT
```

并拒绝执行新请求。

### 14.3 QUERY 重试

相同 `requestId` 且请求内容相同：

- 允许重新执行；
- 可以重新生成查询结果；
- 如果触发新的 Report，则新的 Report 必须生成新的 `reportId`；
- 与原请求相关的 Report 继续携带同一个 `requestId`。

### 14.4 DISCRETE_ACTION 重试

相同 `requestId` 且请求内容相同：

- 不得重复执行业务动作；
- 应返回首次执行生成的完整响应；
- 如果业务需要持久化异步最终结果，具体策略由 Profile 定义。

### 14.5 CONTINUOUS_SESSION 重试

按对应 Profile 的会话、序号、租约规则处理。

### 14.6 SAFETY_STOP 重试

按对应 Profile 定义的安全规则处理，可以允许重复执行。

---

## 15. 幂等记录保存与跨重启

### 15.1 去重记录保存

有 `expiresAt` 的离散动作请求，其幂等记录至少应保留至：

```text
expiresAt + 幂等保护窗口
```

幂等保护窗口固定为 10 分钟。

### 15.2 跨重启

如果设备重启会导致 RAM 幂等记录丢失，则 `DISCRETE_ACTION` 必须至少采用一种：

1. 持久化 `requestId + 请求摘要 + 首次响应`；
2. 为动作请求增加 Profile 级启动会话约束；
3. 其他经评审证明具有等价安全性的机制。

查询类请求允许重启后重新执行。

安全停止类按 Profile 规则处理。

---

## 16. A-BOX 请求响应

Topic：

```text
/zxwl/abox/{deviceId}/response
```

基础响应：

```json
{
  "version": "4.0",
  "timestamp": 1770186088456,
  "data": {
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "code": 200,
    "msg": "operation accepted"
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `requestId` | String | 是 | 原请求 `requestId` |
| `code` | Integer | 是 | 请求处理结果 |
| `msg` | String | 是 | 展示与诊断文本 |
| `result` | Object | 否 | 同步结构化结果 |

无同步结构化结果时省略 `result`，不发送无意义的：

```json
"result": null
```

---

## 17. 200 成功边界

公共协议不统一定义：

```text
200 = 机械动作已经完成
```

`200` 只表示：

> 请求达到对应 Profile 对当前可执行操作明确定义的成功边界。

例如：

```text
wing_control(action=ascend)
200 = A-BOX 已接受并进入该动作执行流程
```

而：

```text
open_locker
200 = 所有目标格口已经反馈确认打开
```

每个 Profile 必须为每一个可执行操作明确登记 `code=200` 的准确含义。

---

## 18. 公共响应码

| code | 名称 | 含义 |
|---:|---|---|
| `200` | `SUCCESS` | 达到 Profile 对当前操作定义的成功边界 |
| `400` | `BUSINESS_FAILED` | Profile 明确定义的业务执行失败 |
| `4001` | `INVALID_REQUEST` | JSON 结构、必填字段、参数、枚举等非法 |
| `4002` | `UNSUPPORTED_COMMAND` | 不支持该 `command` 或可执行操作 |
| `4003` | `BUSY` | 当前业务繁忙或存在互斥操作 |
| `4004` | `RESERVED` | V4 公共协议保留 |
| `4005` | `REQUEST_EXPIRED` | 请求已过期 |
| `4006` | `REQUEST_ID_CONFLICT` | 相同 requestId 对应不同逻辑请求内容 |
| `500` | `MESSAGE_TOO_LARGE` | 无法在单包上限内生成完整响应 |
| `5001` | `INTERNAL_ERROR` | A-BOX 内部异常 |

业务 Profile 可以定义专用响应码，且只能在对应 Profile 内解释。

### 18.1 无法生成可关联响应的非法请求

如果 Payload：

- 无法解析为合法 JSON；
- 存在重复 JSON Key；
- 缺少合法、唯一、可信的 `requestId`；

则设备可以直接丢弃该请求并记录诊断，不要求生成无法可靠关联的 `/response`。

---

## 19. 主动业务上报

Topic：

```text
/zxwl/abox/{deviceId}/report
```

通用结构：

```json
{
  "version": "4.0",
  "timestamp": 1770186089000,
  "data": {
    "reportId": "69c06723-cb44-43db-905b-f912e97fdb50",
    "reportType": "state",
    "reportName": "wing_status",
    "payload": {
      "motionState": "ascending",
      "positionMm": 126
    }
  }
}
```

---

## 20. Report 公共字段

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `reportId` | String | 是 | 本次逻辑报告唯一标识 |
| `reportType` | String | 是 | `state` / `event` / `telemetry` |
| `reportName` | String | 是 | 具体业务上报名称 |
| `instanceId` | String | 否 | 多实例 Profile 使用 |
| `streamId` | String | 条件必填 | 严格有序流使用 |
| `sequence` | Integer | 条件必填 | 严格有序流使用 |
| `requestId` | String | 否 | 与某次请求相关时使用 |
| `payload` | Object / null | 是 | 业务数据 |

---

## 21. reportId

`reportId` 用于逻辑 Report 去重。

规则：

1. 相同逻辑 Report 因 QoS 重投时，`reportId` 必须保持不变。
2. 新生成的 Report 必须使用新的 `reportId`。
3. 同一 `deviceId` 在平台规定的 Report 去重保留期内不得复用相同 `reportId`。
4. 平台按：

```text
deviceId + reportId
```

进行去重。

平台 Report 去重保留期固定为 24 小时。`reportId` 长度和字符集见第 45 节；默认生成方式为 RFC 9562 UUIDv4。资源受限设备可以使用其他生成方式，但必须在 `deviceId` 范围内满足 24 小时不复用的唯一性要求。

---

## 22. reportType

V4 固定定义三类：

### 22.1 state

表示当前业务状态或完整状态快照。

```text
QoS = 1
Retain = false
```

### 22.2 event

表示一次离散业务事件。

```text
QoS = 1
Retain = false
```

### 22.3 telemetry

表示连续、高频、允许少量丢失的数据。

```text
QoS = 0
Retain = false
```

具体 Profile 不得自行增加新的 `reportType`。

新增业务语义应通过新的 `reportName` 表达。

---

## 23. instanceId

如果某 Profile 在同一 A-BOX 中存在多个同类实例，则相关 Report 必须携带 `instanceId`。

Profile 必须定义：

- 是否支持多实例；
- `instanceId` 是否必填；
- 允许字符；
- 最大长度；
- 唯一性范围；
- 请求侧如何在 `params` 中指定同一实例。

平台最新状态主键统一为：

```text
deviceId + reportName + instanceId
```

单实例 Profile 的 `instanceId` 使用固定逻辑空值。

如果 Profile 在 Manifest 中声明了 `instances`，则 Report 中的 `instanceId` 必须属于当前已确认 Manifest 的实例集合。平台收到未声明实例的 Report 时应保存原始消息并标记能力配置不一致，不得自动把该实例加入当前 Manifest。

---

## 24. 严格有序流

普通 Report 不要求顺序号。

仅需要严格顺序的 Profile 使用：

```text
streamId + sequence
```

规则：

1. `streamId` 和 `sequence` 必须同时出现或同时不存在。
2. `sequence` 使用 `uint32`。
3. `sequence` 必须从 `1` 开始递增。
4. 在 `uint32` 回绕之前必须更换 `streamId`。
5. A-BOX 重启或流重新初始化时必须生成新的 `streamId`。
6. 平台只允许在相同：

```text
deviceId + reportName + instanceId + streamId
```

范围内比较 `sequence`。

不同 `streamId` 的 `sequence` 不得直接比较新旧。

---

## 25. 与请求关联的异步 Report

如果某条 Report 是某次平台请求的异步后续结果，必须携带原：

```text
requestId
```

示例：

```json
{
  "version": "4.0",
  "timestamp": 1770186095000,
  "data": {
    "reportId": "b1177af0-26c7-4ce0-8899-21ba7181154d",
    "reportType": "event",
    "reportName": "wing_control_result",
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "payload": {
      "action": "ascend",
      "result": "completed"
    }
  }
}
```

完成、失败、停止等具体结果语义由 Profile 在 `payload` 中定义。

公共协议不定义统一 `executionState`。

---

## 26. 状态恢复原则

由于公共 `/report` 不使用 Retain，V4 同时规定以下三层状态恢复机制：

1. A-BOX 启动完成后主动补发完整状态；
2. MQTT 重新连接成功后主动补发完整状态；
3. 平台持久保存最新状态，并可主动执行公共 `sync_state`。

---

## 27. Profile 完整状态

每个具备状态的 Profile 必须明确登记其完整状态 `reportName`。

完整状态必须能够在不依赖历史消息的情况下，让平台重新建立该 Profile 当前可观察状态。

例如：

```text
wing -> wing_status
refrigeration -> refrigeration_status
locker -> locker_status
```

---

## 28. 公共 sync_state 命令

V4 Core 定义：

```text
command = sync_state
category = QUERY
```

请求：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "requestId": "a820dbf4-d62e-4f51-a389-f881616fa933",
    "command": "sync_state",
    "params": null
  }
}
```

响应：

```json
{
  "version": "4.0",
  "timestamp": 1770186088400,
  "data": {
    "requestId": "a820dbf4-d62e-4f51-a389-f881616fa933",
    "code": 200,
    "msg": "state synchronization scheduled",
    "result": {
      "expectedStates": [
        {
          "reportName": "wing_status"
        },
        {
          "reportName": "refrigeration_status",
          "instanceId": "1"
        },
        {
          "reportName": "refrigeration_status",
          "instanceId": "2"
        }
      ]
    }
  }
}
```

`200` 仅表示：

> A-BOX 已接受并安排本轮状态同步。

不表示：

- Report 已全部发布；
- Broker 已全部收到；
- Kafka 已全部收到；
- 平台业务逻辑已完成处理。

### 28.1 expectedStates

`result.expectedStates` 是本轮状态同步的权威预期集合。

规则：

1. 每个条目必须包含 `reportName`。
2. 单实例状态条目不得携带 `instanceId`。
3. 多实例状态条目必须携带 `instanceId`。
4. `expectedStates` 中 `(reportName, instanceId)` 组合必须唯一，不得重复。
5. 平台不得仅根据 Manifest Profile 数量推测本轮应等待多少状态。
6. A-BOX 第一次处理某个 `sync_state.requestId` 时确定该请求对应的 `expectedStates`；在该逻辑同步请求的后续相同 `requestId` 重试中不得改变此集合。

### 28.2 sync_state 触发的 Report

本次 `sync_state` 触发的所有完整状态 Report 必须携带该次：

```text
requestId
```

平台根据：

```text
requestId + expectedStates
```

判断本轮同步是否完整。

同一 `requestId` 下，同一个状态键：

```text
reportName + instanceId
```

可以因 `sync_state` 重试收到多条完整状态 Report。

这些 Report 必须使用不同的 `reportId`。平台应以**最后接收到的完整状态快照**作为本轮该状态键的结果。

### 28.3 sync_state 重试

如果平台使用相同 `requestId`、相同请求内容重试 `sync_state`：

- A-BOX 必须沿用该 `requestId` 首次确定的 `expectedStates`；
- A-BOX 重新发送 `expectedStates` 中对应的完整状态；
- 新生成的每条 Report 必须使用新的 `reportId`；
- 新生成的 Report 继续携带相同 `requestId`；
- 不得因为当前 Manifest 或运行时能力发生变化而修改同一 `requestId` 对应的 `expectedStates`。

如果 Manifest 在同步过程中发生变化：

- 平台必须废弃尚未完成的旧同步轮次；
- 等待新 Manifest 完成在线确认；
- 使用新的 `requestId` 发起新的 `sync_state`；
- A-BOX 以新请求建立新的 `expectedStates`。

### 28.4 Report 早于 sync_state 响应到达

由于 MQTT Bridge、Kafka 或平台内部链路可能改变不同 Topic 消息的到达先后，平台可能先收到携带某个 `requestId` 的状态 Report，之后才收到对应 `sync_state` 响应。

平台应：

- 按 `requestId` 临时缓存提前到达的完整状态 Report；
- 收到 `sync_state` 响应后，根据 `expectedStates` 对缓存 Report 进行匹配；
- 不得仅因为响应晚于 Report 到达而丢弃合法同步结果。

### 28.5 sync_state 等待超时

平台从收到成功响应开始等待本次 `sync_state` 的完整状态，固定超时为 15 秒。

同步超时只表示：

> 本轮状态同步未完整收齐预期状态。

同步超时不得直接把未收到的业务状态解释为：

- 业务设备离线；
- 设备故障；
- 已停止；
- 已关闭；
- 正常；
- 其他确定性业务状态。

平台可以保留此前持久化状态，并将本轮同步结果标记为“不完整”或“未知新鲜度”。

---

## 29. A-BOX 自动状态补发

A-BOX 在以下场景必须主动补发所有当前已启用 Profile 的完整状态：

1. A-BOX 启动完成，业务模块已经获得可靠状态；
2. MQTT 连接重新建立后。

此类自动补发没有平台请求，因此：

```text
requestId
```

省略。

---

## 30. 心跳

Topic：

```text
/zxwl/abox/{deviceId}/heartbeat
```

QoS：

```text
0
```

Retain：

```text
false
```

基础示例：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "firmware": "abox-4.0.0"
  }
}
```

如果当前产品要求通过 `manifestId` 绑定在线实例与能力清单，则：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "firmware": "abox-4.0.0",
    "manifestId": "sha256:..."
  }
}
```

完整能力清单不得放入心跳。

---

## 31. 心跳语义

心跳只证明：

> 当前 A-BOX 与 MQTT Broker 之间的通信处于存活状态。

心跳不证明：

- 飞翼控制器正常；
- 冷机正常；
- 格口设备正常；
- 底盘可控；
- CAN 正常；
- RS485 正常；
- 整车业务功能处于安全可用状态。

具体业务设备状态必须通过 `/report` 表达。

### 31.1 平台动作下发前置条件

平台只应向同时满足以下条件的 A-BOX 下发 `DISCRETE_ACTION`、`CONTINUOUS_SESSION` 等动作类请求：

1. 当前心跳判定为在线；
2. 当前在线实例的 Manifest 已完成确认；
3. 已确认 Manifest 声明支持目标 Profile；
4. 平台支持该 Profile 的当前 MAJOR 版本；
5. 所使用的具体操作在设备声明的 Profile 版本能力范围内。

该检查不能消除“平台判断在线后、实际发布前设备立即掉线”的网络竞态，因此设备端仍必须执行 `expiresAt`、幂等、租约和 Profile 安全状态机等保护。

---

## 32. 心跳周期与离线判定

固定值：

```text
heartbeatInterval = 5s
offlineTimeout = 15s
```

平台在线判定必须依据实际收到心跳的时间。

不得使用设备 `timestamp` 替代平台接收时间。

心跳周期不得由 Profile 或项目改写。设备不主动为 5 秒周期增加协议级随机抖动；网络和调度造成的少量发送抖动不改变平台连续 15 秒未收到心跳即离线的判定。

---

## 33. Manifest

Topic：

```text
/zxwl/abox/{deviceId}/manifest
```

QoS：

```text
1
```

Retain：

```text
true
```

Manifest 用于：

- 核对设备实际业务能力；
- 核对 Profile 版本；
- 检查资产台账和设备实际能力是否一致；
- 固件升级或配置变化后识别能力变化。

Manifest 不参与逐条业务请求或 Report 路由。

---

## 34. Manifest 结构

示例：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "manifestId": "sha256:...",
    "firmware": "abox-4.0.0",
    "profiles": [
      {
        "name": "refrigeration",
        "version": "1.2",
        "instances": ["1", "2"]
      },
      {
        "name": "wing",
        "version": "1.0"
      }
    ]
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `manifestId` | String | 条件必填 | 规范化 Manifest 内容摘要 |
| `firmware` | String | 是 | 当前 A-BOX 固件版本 |
| `profiles` | Array | 是 | 当前实际启用的 Profile |
| `profiles[].name` | String | 是 | Profile 名称 |
| `profiles[].version` | String | 是 | Profile 版本，格式为 `MAJOR.MINOR` |
| `profiles[].instances` | Array&lt;String&gt; | 条件必填 | 多实例 Profile 的完整实例集合 |

Manifest 中：

- `profiles[].name` 必须唯一，不得重复声明同名 Profile；
- 单实例 Profile 必须省略 `instances`；
- 多实例 Profile 必须声明完整 `instances`；
- 同一 Profile 的 `instances` 内实例标识必须唯一；
- `instances` 不是逐条消息路由模型，只用于能力与实例配置声明；
- 多实例 Profile 的 `instanceId` 必须来自 Manifest 当前声明的实例集合；
- 实例集合发生增加、删除或标识变化时，必须重新发布 Manifest；
- 实例集合变化属于 Manifest 内容变化，因此必须产生新的 `manifestId`。

---

## 35. Profile 版本

Profile 版本统一使用：

```text
MAJOR.MINOR
```

例如：

```text
1.0
1.1
2.0
```

`MAJOR` 和 `MINOR` 都是 `uint16` 十进制数，不得使用前导零；允许范围为 `0..65535`。

规则：

### 35.1 MAJOR

MAJOR 变化表示存在不兼容变化，例如：

- 修改既有字段语义；
- 修改既有必填字段；
- 删除既有能力；
- 修改既有命令成功边界；
- 修改既有 Report 基本语义。

### 35.2 MINOR

MINOR 只允许向后兼容扩展，例如：

- 新增 command；
- 新增 reportName；
- 新增可忽略可选字段；
- 新增业务专用响应码。

### 35.3 平台兼容规则

如果平台不支持设备声明的 Profile MAJOR：

- 不得向该 Profile 发送动作类请求；
- 可以保存未知 Report 原始数据；
- 不得把该能力标记为已确认可控。

如果 MAJOR 相同、设备 MINOR 高于平台：

- 平台使用已知兼容子集；
- 忽略未知兼容扩展。

如果设备 MINOR 低于平台：

- 平台不得使用高于设备 MINOR 才引入的能力。

Manifest 尚未确认时，不得把其中的业务能力视为当前在线实例“已确认可用”。

---

## 36. manifestId

### 36.1 何时必须存在

如果同一个 `firmware` 版本可能因为：

- 配置；
- 硬件装配；
- 参数开关；
- 项目差异；

对应不同的 Profile 集合、Profile 版本或 `instances` 集合，则 `manifestId` 必须存在，并由心跳回显。

只有当：

> 同一 firmware 唯一决定全部 Profile、Profile 版本及 `instances` 集合

时，才可以省略 `manifestId`。

### 36.2 摘要输入

`manifestId` 必须是 Manifest `data` 中除 `manifestId` 字段自身之外**全部字段**的规范化内容摘要。

V4 摘要输入精确为：

```json
{
  "firmware": "...",
  "profiles": [...]
}
```

如果未来在同一 V4 兼容扩展中向 Manifest `data` 增加新的字段，该字段默认必须参与 `manifestId` 计算，除非公共协议明确声明该字段为“不参与摘要字段”。

外层：

```text
version
timestamp
```

不参与 `manifestId`。

### 36.3 固定算法

V4 固定使用：

```text
manifestId = "sha256:" + lowercase_hex(SHA-256(canonical_manifest_data))
```

字符串格式：

```text
sha256:<64位小写十六进制>
```

V4 不允许实现方自行选择其他摘要算法。

### 36.4 Canonicalization

V4 JSON Canonicalization 固定采用“语义集合归一化 + RFC 8785 JCS”：

- 解析阶段拒绝重复 Key、非法数字和不符合字段合同的值；
- Manifest 的 `profiles` 数组先按 `name` 的 Unicode 码点顺序稳定排序；
- `profiles[].instances` 先按实例标识的 Unicode 码点顺序稳定排序，重复实例非法；
- 其他数组保持原顺序，只有 Core 或 Profile 明确声明为集合的数组才可在 JCS 前排序；
- 随后执行 RFC 8785 JCS：对象递归排序、UTF-8 无 BOM、无额外空白并使用 JCS 字符串转义；
- 不执行 Unicode Normalization；`null` 保留为 JSON `null`；
- V4 公共数字值只允许安全整数，不允许小数、指数形式、负零、`NaN` 或无穷值。

Manifest Canonicalization 必须保证：

- Profile 原始排列顺序不同但内容相同，摘要结果一致；
- 多实例 `instances` 原始排列顺序不同但集合相同，摘要结果一致；
- `instances` 中出现重复值属于非法 Manifest，不得进入摘要计算。

固定测试向量位于 `docs/protocols/v4/test-vectors.json`，只读校验器为 `tools/validate_mqtt_v4.py`，覆盖：

```text
Manifest JSON
→ Canonical Bytes
→ SHA-256
```

测试向量以及重复 Key、Unicode、对象乱序、普通数组换序和非法数值。

---

## 37. Manifest 发布和在线确认

A-BOX 必须在以下情况发布 Manifest：

1. MQTT 连接建立后；
2. 固件版本变化后；
3. Profile 集合变化后；
4. Profile 版本变化后；
5. 影响能力声明的配置变化后。

Retained Manifest 只代表最近一次设备声明，不证明设备当前在线。

Manifest 在线确认规则：

1. **使用 `manifestId` 的产品**：平台只有在当前在线心跳中收到与 Retained Manifest 相同的 `manifestId` 后，才能确认该 Manifest 对应当前在线实例。
2. **省略 `manifestId` 的产品**：该产品必须满足“同一 `firmware` 唯一决定全部 Profile 及其版本”。平台只有在当前在线心跳中的 `firmware` 与 Retained Manifest 中的 `firmware` 完全一致后，才能确认该 Manifest 对应当前在线实例。
3. Manifest 尚未完成上述在线确认时，平台不得把其中能力标记为当前在线实例已确认可用。

---

## 38. MQTT 重连后的发布顺序

A-BOX 成功重新连接 Broker 后，逻辑执行顺序必须为：

```text
1. 发布 Manifest
2. 启动 / 恢复 Heartbeat
3. 补发所有完整 State
4. 进入正常业务运行
```

该顺序用于设备端发送逻辑。

平台不得假定不同 Topic 经 Broker Bridge、Kafka 等后续链路后仍保持严格到达顺序。

如果业务 Report 先于 Manifest 确认进入平台：

- 可以保存或缓存；
- 不得据此确认新的业务能力已经可用。

---

## 39. command / reportName / Profile 注册治理

全平台必须维护稳定的：

```text
Command Registry
ReportName Registry
Profile Registry
```

### 39.1 全局唯一性

V4 公共业务命名空间中：

- `command` 必须全局唯一；
- `reportName` 必须全局唯一；
- 不允许不同 Profile 注册同名但不同语义的 `command`；
- 不允许不同 Profile 注册同名但不同语义的 `reportName`；
- `sync_state` 等 Core 定义的命令属于保留名称，任何 Profile 不得覆盖或重新解释；
- 同一 A-BOX 上不得存在路由含义冲突的注册项。

由于普通业务请求中不携带 Profile 标识，A-BOX 必须能够仅依据：

```text
command + params
```

在执行前唯一确定目标业务操作。

### 39.2 Registry 最小登记信息

Command Registry 至少记录：

```text
command
ownerProfile
introducedVersion
semanticDescription
```

ReportName Registry 至少记录：

```text
reportName
ownerProfile
introducedVersion
semanticDescription
```

Profile Registry 至少记录：

```text
profileName
supportedMajorVersions
currentVersion
```

### 39.3 operationMatcher 唯一匹配

同一个 `command` 可以通过多个 `operationMatcher` 区分不同可执行操作。

在实际执行前：

- 必须且只能命中一个 `operationMatcher`；
- 命中 0 个时返回 `4001 INVALID_REQUEST`；
- 同时命中多个时返回 `4001 INVALID_REQUEST`；
- 以上两种情况均不得执行任何业务动作。

Profile 设计时不得依赖“匹配顺序”消除歧义。

### 39.4 命名规则

命名规则至少要求：

- 小写；
- 单词使用下划线连接；
- 不使用车型名、客户名、项目名作为业务命名前缀；
- 相同业务语义必须复用已有名称。

例如不应存在：

```text
vehicle_a_wing_control
vehicle_b_wing_control
```

如果语义一致，应统一：

```text
wing_control
```

`command`、`reportName` 和 Profile 名称最大长度均为 64 个字符。

---

## 40. Profile 操作登记表

每个 Profile 必须为所有可执行操作维护至少以下信息：

| 字段 | 说明 |
|---|---|
| `command` | 命令名称 |
| `operationMatcher` | 参数条件，用于在执行前确定操作 |
| `category` | QUERY / DISCRETE_ACTION / CONTINUOUS_SESSION / SAFETY_STOP |
| `params` | 参数 Schema |
| `expiresAt` | 必填 / 可选 / 特殊规则 |
| `maxValidity` | 允许的最大请求有效期 |
| `200Boundary` | `code=200` 的精确业务含义 |
| `result` | 同步返回定义 |
| `rebootPolicy` | 跨重启策略 |
| `errorCodes` | 专用响应码 |

如果整个 `command` 的所有参数组合都属于同一类别，可以省略 `operationMatcher`。

---

## 41. Profile Report 登记表

每个 Profile 必须为 Report 维护至少：

| 字段 | 说明 |
|---|---|
| `reportName` | 上报名称 |
| `reportType` | state / event / telemetry |
| `payload` | Payload Schema |
| `fullState` | 是否属于完整状态 |
| `instanceId` | 是否要求；多实例时必须与 Manifest `instances` 规则一致 |
| `orderedStream` | 是否要求 streamId + sequence |
| `requestCorrelation` | 是否可能携带 requestId |

Report QoS 由公共 `reportType` 固定，不允许 Profile 单独降低或提升。

---

## 42. 平台状态主键

平台保存最新 `state` 时，逻辑状态主键统一为：

```text
deviceId + reportName + instanceId
```

单实例 Profile 的 `instanceId` 使用固定逻辑空值。

---

## 43. 平台处理边界

平台内部可以选择任意消息队列、存储和服务拆分方式，但不得改变下列协议语义：

1. 先校验公共外层，再按 `command` 或 `reportType + reportName` 路由；
2. 按 `deviceId + reportId` 执行 24 小时 Report 去重；
3. 按第 42 节逻辑主键保存最新 `state`；
4. 不假定不同 MQTT Topic 经过中间链路后仍保持到达顺序；
5. 心跳在线判定使用 Broker 接收时间，不与业务 Report 状态混为一体；
6. 未识别的兼容字段和枚举按第 6.3、6.4 节处理，不得由平台内部实现重新解释公共语义。

---

## 44. MQTT 传输、设备身份、deviceId 与 ACL

生产环境允许明文 MQTT 或 MQTT over TLS；两种传输方式的设备身份、独立凭据与 ACL 要求相同。是否使用 TLS 由产品部署决定，不改变公共 Topic 和报文格式。

### 44.1 独立设备凭据

每台 A-BOX **必须使用独立设备凭据**。

禁止多台生产 A-BOX 共享同一可写公共业务凭据。

已认证设备身份必须在服务端唯一绑定到一个：

```text
deviceId
```

Broker 和平台不得仅信任客户端在 Topic 或 Payload 中自行声明的设备身份。

### 44.2 deviceId 字符约束

V4 的 `deviceId` 仅允许：

```text
A-Z
a-z
0-9
_
-
```

必须使用以下完整校验表达式：

```text
^[A-Za-z0-9_-]+$
```

明确禁止：

```text
/
+
#
空白字符
控制字符
```

`deviceId` 长度固定为 1～32 个 ASCII 字符。

### 44.3 身份绑定与 ACL

Broker ACL 必须基于“已认证设备身份 → 唯一 deviceId”的服务端绑定生成。

A-BOX 只能发布自身：

```text
/zxwl/abox/{deviceId}/response
/zxwl/abox/{deviceId}/report
/zxwl/abox/{deviceId}/heartbeat
/zxwl/abox/{deviceId}/manifest
```

A-BOX 只能订阅自身：

```text
/zxwl/abox/{deviceId}/request
```

不得允许一台 A-BOX：

- 发布其他 `deviceId` 的消息；
- 订阅其他 `deviceId` 的请求；
- 使用 MQTT 通配符扩大自身授权范围；
- 通过构造 Topic 字符串绕过身份绑定。

### 44.4 MQTT ClientId

MQTT `ClientId` 必须逐字等于 `deviceId`。Broker 必须把已认证设备凭据、`ClientId` 和 Topic ACL 三者绑定；不得仅凭客户端提供的 `ClientId` 授权。

使用 TLS 时最低为 TLS 1.2，支持时优先 TLS 1.3；设备必须校验服务端证书链、主机名和证书有效时间，并发送正确 SNI。使用明文 MQTT 时，链路不提供保密性和传输层防篡改能力，部署方必须明确接受这一风险，不得把明文连接标识为 TLS 连接。每台设备在两种传输方式下都必须使用独立 Broker 凭据；凭据轮换属于受控配置或生产 provisioning，不通过公共业务 Topic 传输，切换完成后必须撤销旧凭据。

---

## 45. 报文大小和字段限制

| 项目 | 正式上限或格式 |
|---|---|
| MQTT 单条 Payload | 完整 UTF-8 JSON 最大 1024 字节，不含 Topic 和 MQTT 包头；禁止 BOM |
| `params` / `result` / `payload` | 各自序列化后最大 768 字节，同时仍受整包 1024 字节限制 |
| `requestId` | 1～64 个 ASCII 字符，`^[A-Za-z0-9][A-Za-z0-9._:-]*$` |
| `reportId` | 1～96 个 ASCII 字符，同上 |
| `streamId` | 1～64 个 ASCII 字符，同上 |
| `instanceId` | 1～32 个 ASCII 字符，`^[A-Za-z0-9_-]+$` |
| `command` / `reportName` / Profile 名称 | 最长 64 个字符，小写 snake_case |
| `firmware` | 最长 64 个 ASCII 字符 |
| `msg` | 最长 128 个 UTF-8 字节，禁止控制字符 |
| Manifest | 最多 16 个 Profile |
| Report 去重 | `deviceId + reportId` 保存 24 小时 |
| 请求幂等保护 | 至少保存至 `expiresAt + 10 分钟` |

超出公共单包上限的数据不得自行在公共业务 Topic 中分片。

大文件、日志、固件等继续使用 `/internal/*` 对应的数据通道。

---

## 46. 自动重连

A-BOX 必须实现 MQTT 自动重连，并使用以下固定退避规则：

1. 首次连接随机延迟 0～4 秒；
2. 连续失败的退避上限依次为 4、8、16、32、60 秒；
3. 每次实际延迟从当前上限的 50%～100% 取带设备差异的随机抖动；
4. 后续失败保持 60 秒上限；
5. 稳定在线 5 分钟后重置退避级别。

设备差异抖动必须避免大量设备在同一固定时间点同步重连 Broker。

---

## 47. V4 兼容性规则

以下变化可以继续使用：

```text
version = "4.0"
```

- 新增 Profile；
- 新增 command；
- 新增 reportName；
- Profile MINOR 兼容扩展；
- `params` 增加可忽略可选字段；
- `payload` 增加可忽略可选字段；
- 新增 Profile 专用响应码；
- Manifest 增加兼容可选字段。

以下变化必须升级公共协议主版本：

- 修改公共字段名称；
- 修改公共字段类型；
- 修改公共字段必填性；
- 修改既有公共字段语义；
- 修改五个公共 Topic 的基本资源模型；
- 修改 `requestId` 基本幂等语义；
- 修改 `reportId` 去重语义；
- 修改四种操作类别基本语义；
- 修改 `reportType` 基本语义；
- 修改 MQTT 会话安全边界。

---

## 48. V3 → V4 关系

V4 是公共业务协议主版本升级，不属于 V3 透明兼容扩展。

主要变化：

1. 不再假设一台 A-BOX 只有一个固定 `deviceType`；
2. 请求统一 `/request`；
3. 响应统一 `/response`；
4. 主动业务上报统一 `/report`；
5. 心跳继续独立 `/heartbeat`；
6. 新增 `/manifest`；
7. 控制路由使用 `command`；
8. 上报路由使用 `reportType + reportName`；
9. 引入 Manifest Profile 版本治理；
10. 增加操作分类、请求过期、跨重启安全要求；
11. 增加统一状态恢复 `sync_state`；
12. 增加按需的 `instanceId`、`streamId + sequence` 和异步 `requestId`。

迁移按项目整批切换：每个项目的平台 Handler 与该项目全部目标设备同时切换，一个设备固件只运行一个公共协议版本，不实现 V3/V4 双栈。

跨项目迁移期间，共用平台同时保留尚未迁移项目的 V3 Handler 和已迁移项目的 V4 Handler，并按收到的协议版本、心跳和 Manifest 明确选择；当最后一个项目迁移完成后才删除 V3 Handler。平台只有在收到 V4 心跳且确认对应 Manifest 后才允许下发 V4 动作，协议状态不明确时禁止动作下发。

---

## 49. V4 公共模型汇总

### Request

```text
version
timestamp
data
├─ requestId
├─ command
├─ expiresAt?
└─ params
```

### Response

```text
version
timestamp
data
├─ requestId
├─ code
├─ msg
└─ result?
```

### Report

```text
version
timestamp
data
├─ reportId
├─ reportType
├─ reportName
├─ instanceId?
├─ streamId?   ┐
├─ sequence?   ┘ 必须成对
├─ requestId?
└─ payload
```

### Heartbeat

```text
version
timestamp
data
├─ firmware
└─ manifestId?   // 能力不能由 firmware 唯一确定时要求
```

### Manifest

```text
version
timestamp
data
├─ manifestId?
├─ firmware
└─ profiles[]
```

---

## 50. 正式冻结参数汇总

V4.0 架构和实现性参数均已冻结：

1. 完整 MQTT Payload 最大 1024 字节；内容对象最大 768 字节；
2. 标识、命名和展示字段长度按第 45 节；
3. `deviceId` 为 1～32 个指定 ASCII 字符，`ClientId` 必须与其相同；
4. 心跳固定 5 秒，平台连续 15 秒未收到即离线；
5. `DISCRETE_ACTION` 公共有效期上限 30 秒，Profile 只能更短；
6. 请求幂等保护至 `expiresAt + 10 分钟`；
7. Report 去重保留 24 小时；
8. 生产可使用明文 MQTT 或最低 TLS 1.2，每设备均使用独立凭据；
9. 重连使用 4/8/16/32/60 秒上限和 50%～100% 抖动；
10. Canonicalization 使用语义集合归一化后执行 RFC 8785 JCS；
11. Manifest 摘要固定使用 SHA-256，固定向量随协议归档；
12. 请求摘要固定使用 SHA-256，固定向量随协议归档；
13. Epoch 和安全整数边界、词法及非法值固定向量随协议归档；
14. `sync_state` 等待超时固定为 15 秒；
15. Command、ReportName 和 Profile 以机器可校验 Registry 为唯一登记入口；
16. 平台跨项目双栈，设备按项目整批迁移且单版本运行。

以上冻结参数不得重新改变：

```text
五 Topic
+
Request / Response / Report / Heartbeat / Manifest 公共模型
+
command 控制路由
+
reportType/reportName 上报路由
+
Manifest 能力治理
```

这一整体架构。

除非发现不可实现的安全缺陷，否则后续评审不得重新引入强制 Component Resource Model 或重新拆分公共 Topic。

---

## 51. 最终设计目标

V4 应保证：

```text
新增车型
    ≠ 修改公共协议

改变车辆设备组合
    ≠ 修改公共协议

新增业务能力
    = 新增或升级 Profile

新增控制能力
    = 新增 command / 可执行操作

新增主动业务数据
    = 新增 reportName

底层 CAN / RS485 / 执行器变化
    ≠ 修改平台公共协议
```

即：

> **公共协议只解决平台与 A-BOX 如何可靠、安全、一致地通信。**

> **业务 Profile 只解决具体操作做什么，以及 A-BOX 向平台报告什么。**
