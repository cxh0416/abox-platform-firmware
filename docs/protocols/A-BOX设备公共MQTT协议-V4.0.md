# A-BOX 设备公共 MQTT 协议

文档修订：V4.0（正式冻结）

报文版本：`4.0`

修订日期：2026-09-16

## 1. 协议边界

本文定义 A-BOX 公共业务 MQTT 契约：身份、Topic、报文外层、QoS、响应码、幂等、状态同步、Manifest、兼容和安全要求。设备命令、状态字段及其安全语义由已登记的业务 Profile 定义。

设备内部总线、执行器拓扑、硬件映射、控制算法、运维接口和测试记录不属于公共协议。配置、日志、OTA 和设备信息继续使用独立的 `/internal/*` 协议，不得混入 V4 公共业务报文。

V3 是存量迁移协议；V4 与 V3 的 Topic 和报文不可混用。一个设备固件只运行一个公共协议版本。

## 2. 身份和传输安全

平台为每台设备分配全局唯一 `deviceId`。其长度为 1～32 个 ASCII 字符，必须匹配 `^[A-Za-z0-9_-]+$`。MQTT ClientId 必须逐字等于 `deviceId`，Broker 必须把认证凭据、ClientId 和该设备 Topic ACL 绑定。

生产连接必须满足：

- TLS 1.2 或更高版本；支持时优先 TLS 1.3；
- 校验服务端证书链、主机名、证书有效时间和 SNI；
- 每台设备使用独立凭据，禁止跨设备共享；
- 禁止对生产端点关闭证书或主机名校验。

隔离实验 Broker 只有在显式非生产配置下才可使用明文 MQTT，该例外不得用于生产端点。

MQTT 3.1.1 固定 `Clean Session=true`；MQTT 5 固定 `Clean Start=true` 且 `Session Expiry Interval=0`。设备每次连接成功后重新订阅，不依赖 Broker 保存旧会话或离线命令。

## 3. Topic

`{deviceId}` 按分配值原样替换。V4 只定义以下五个公共 Topic：

| 方向 | Topic | QoS | Retain | 用途 |
|---|---|---:|---:|---|
| 平台 → A-BOX | `/zxwl/abox/{deviceId}/request` | 1 | 否 | Core 或 Profile 命令 |
| A-BOX → 平台 | `/zxwl/abox/{deviceId}/response` | 1 | 否 | 请求的最终响应 |
| A-BOX → 平台 | `/zxwl/abox/{deviceId}/report` | 1 | 否 | 离散状态和事件 |
| A-BOX → 平台 | `/zxwl/abox/{deviceId}/heartbeat` | 0 | 否 | 在线心跳 |
| A-BOX → 平台 | `/zxwl/abox/{deviceId}/manifest` | 1 | 是 | 当前能力清单 |

设备只订阅自身精确的 `request` Topic。平台不得在 V4 上使用 V3 的 `/ctrl`、`/status` 或 `deviceType` 路由。

## 4. 通用限制

- Payload 必须是无 BOM 的完整 UTF-8 JSON，最大 1024 字节，不含 Topic 和 MQTT 包头。
- `params`、`result`、`payload` 各自序列化后最大 768 字节，同时仍受整包 1024 字节限制。
- JSON 对象禁止重复 Key；禁止 `NaN`、`Infinity` 和负零。
- 公共数字字段只允许安全整数，范围为 `-9007199254740991..9007199254740991`。
- Epoch 字段只允许十进制非负整数词法，不接受字符串、小数、指数形式或前导 `+`。
- 未识别的兼容扩展字段必须保留并忽略；未知枚举必须保留原值并按 `unknown` 降级展示。

标识和展示字段限制如下：

| 字段 | 约束 |
|---|---|
| `requestId` | 1～64 个 ASCII 字符，`^[A-Za-z0-9][A-Za-z0-9._:-]*$` |
| `reportId` | 1～96 个 ASCII 字符，同上；推荐 RFC 9562 UUIDv4 |
| `streamId` | 1～64 个 ASCII 字符，同上 |
| `instanceId` | 1～32 个 ASCII 字符，`^[A-Za-z0-9_-]+$` |
| `command`、`reportName`、Profile 名称 | 最长 64 个字符，小写 snake_case |
| `firmware` | 最长 64 个 ASCII 字符 |
| `msg` | 最长 128 个 UTF-8 字节，禁止控制字符 |
| Profile 版本 | `MAJOR.MINOR`；每段为无前导零的 `uint16` 十进制数 |

## 5. 报文外层

所有公共报文都使用下列外层，不包含 `deviceType`：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {}
}
```

`timestamp` 是发送端生成的 UTC Unix Epoch 毫秒整数。在线和超时判定使用 Broker 接收时间，不使用设备时间戳替代。

## 6. 请求

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "command": "example_command",
    "params": null,
    "expiresAt": 1770186098000
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `data.requestId` | String | 是 | 全局唯一请求标识 |
| `data.command` | String | 是 | Registry 中登记的 Core 或 Profile 命令 |
| `data.params` | Object / null | 是 | 命令参数；无参数时为 `null` |
| `data.expiresAt` | Integer | 由命令分类决定 | UTC Epoch 毫秒截止时间 |

命令分类：

- `DISCRETE_ACTION`：`expiresAt` 必填；接收时已经过期则返回 `4005`，不得执行。有效期不得超过 30 秒，Profile 可规定更小值。
- `QUERY`：`expiresAt` 可选；存在时仍须执行过期检查。
- `CORE`：由本协议定义具体规则。

## 7. 响应

```json
{
  "version": "4.0",
  "timestamp": 1770186088456,
  "data": {
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "code": 200,
    "msg": "operation completed",
    "result": {}
  }
}
```

`result` 仅在命令需要机器可读结果时出现；禁止发送无意义的 `result:null`。`msg` 仅用于展示和诊断，业务判断以 `code` 和 Profile 定义的 `result` 为准。

| 响应码 | 含义 |
|---:|---|
| `200` | 按 Core 或 Profile 定义的最终成功边界完成 |
| `400` | Profile 已定义的业务执行失败 |
| `4001` | JSON、字段或参数非法，或相同 `requestId` 内容冲突 |
| `4002` | 当前 Manifest 不支持该命令 |
| `4003` | 业务资源或控制流程繁忙 |
| `4005` | 请求已过期或有效期超过命令上限 |
| `500` | 完整响应无法在单包上限内生成 |
| `5001` | A-BOX 内部异常 |

## 8. 请求幂等和摘要

设备必须保存 `requestId`、请求摘要、执行状态和完整最终响应：

- 相同 `requestId` 且摘要相同：返回首次完整最终响应，不重复执行；
- 相同 `requestId` 但摘要不同：返回 `4001`，不得执行；
- `DISCRETE_ACTION` 的记录至少保存至 `expiresAt + 10 分钟`；
- `QUERY` 重试可以重新读取当前状态，但不得把同一 `requestId` 用于不同内容。

请求摘要逻辑对象固定为 `command + params + expiresAt?`，不含报文 `timestamp`。先执行 Profile 声明的集合归一化，再按第 13 节规范化并计算 SHA-256。

需要跨重启幂等的动作由 Profile 明确声明。此类动作必须在驱动执行器前持久化摘要和执行状态；无法持久化时不得执行。

## 9. Report

```json
{
  "version": "4.0",
  "timestamp": 1770186089456,
  "data": {
    "reportId": "b88ec1d0-1951-4cb3-9ea2-23fe54867665",
    "reportName": "example_status",
    "reportType": "state",
    "payload": {}
  }
}
```

`reportType` 允许 `state` 或 `event`。由请求触发的 Report 可以带原 `requestId`，自动状态补发不得伪造请求关联。平台按 `deviceId + reportId` 去重 24 小时；重复投递不得重复触发业务副作用。

## 10. Manifest

Manifest 最多列出 16 个 Profile，并按 Profile 名称、版本和可选实例声明设备当前能力：

```json
{
  "version": "4.0",
  "timestamp": 1770186088000,
  "data": {
    "profiles": [
      {"name": "locker", "version": "1.0"}
    ]
  }
}
```

Manifest 使用 QoS 1、Retain。平台必须先收到 V4 心跳并确认 Manifest 与 Registry 兼容，才允许下发 V4 动作。能力不匹配或协议状态不明确时禁止下发动作。

同一固件的 Profile 集合恒定时可省略 `manifestId`。若同一固件可能启停 Profile 或改变实例集合，则必须：

- 对 Profile 语义归一化后的 Manifest 数据计算 SHA-256；
- `manifestId` 固定为 `sha256:` 加 64 位小写十六进制；
- Manifest 与心跳都携带相同 `manifestId`。

## 11. 心跳和在线判定

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "firmware": "meal-delivery-4.0.0"
  }
}
```

设备每 5 秒发布一次心跳。平台连续 15 秒未收到即判定离线，以 Broker 接收时间为准。Retain Manifest 和最后状态不证明设备在线，心跳也不证明下游控制器或机械动作正常。

## 12. Core 命令 `sync_state`

平台可发送：

```json
{
  "version": "4.0",
  "timestamp": 1770186088333,
  "data": {
    "requestId": "state-sync-001",
    "command": "sync_state",
    "params": null
  }
}
```

设备接受同步后返回：

```json
{
  "version": "4.0",
  "timestamp": 1770186088456,
  "data": {
    "requestId": "state-sync-001",
    "code": 200,
    "msg": "state synchronization started",
    "result": {
      "expectedStates": [
        {"reportName": "example_status"}
      ]
    }
  }
}
```

随后设备发布列出的完整状态 Report，并携带原 `requestId`。平台从成功响应开始等待 15 秒；超时只表示同步不完整，不得据此推断设备或未到达的业务状态。

## 13. Canonicalization 和 SHA-256

规范化顺序固定为：

1. 解析 JSON 并拒绝重复 Key、非法数字和不符合字段合同的值；
2. 执行 Core 或 Profile 明确声明的集合排序、去重等语义归一化；
3. 对逻辑对象执行 RFC 8785 JSON Canonicalization Scheme（JCS）；
4. 对规范 UTF-8 字节计算 SHA-256。

JCS 不执行 Unicode Normalization；对象 Key 递归排序；普通数组保持顺序。只有被 Core 或 Profile 明确声明为集合的数组才可在第 2 步重排。

规范测试向量位于 `docs/protocols/v4/test-vectors.json`，只读校验器为 `tools/validate_mqtt_v4.py`。

## 14. 重连

- 首次连接随机延迟 0～4 秒；
- 连续失败的退避上限依次为 4、8、16、32、60 秒；
- 每次实际延迟从当前上限的 50%～100% 取带设备差异的抖动；
- 稳定在线 5 分钟后重置退避级别；
- 重连后重新订阅、发布当前 Manifest，并按 Profile 要求补发完整状态。

## 15. 迁移规则

V3 到 V4 按项目整批切换：该项目的平台 Handler 与该项目全部目标设备同时切换，设备不实现 V3/V4 双栈。跨项目迁移期间，共用平台同时保留未迁移项目的 V3 Handler 和已迁移项目的 V4 Handler；最后一个项目迁移完成后才可删除 V3。

平台以收到的协议版本、心跳和 Manifest 选择 Handler，不得仅凭资产备注猜测。V3 设备继续使用 V3 Topic；V4 设备只使用本协议五个公共 Topic。

## 16. Registry

机器可校验 Registry 是 Core 命令、Profile、命令和 Report 名称的唯一登记入口。未登记能力不得作为已冻结 V4 能力发布。Registry 只登记已完成 Profile 冻结的项目，不从 V3 文档推导 V4 能力。

## 17. 接入检查

- `version` 必须逐字为 `"4.0"`，公共报文不得包含 `deviceType`；
- Topic、QoS、Retain 和 Payload 上限必须符合本协议；
- 平台重试必须复用原 `requestId`；
- 动作过期、幂等冲突和 Manifest 不匹配时禁止执行；
- 时间戳保持 UTC Epoch 毫秒，不得在公共报文中增加时区偏移；
- 未知字段和枚举按兼容规则处理；
- 公共业务平台不得调用 `/internal/*`。
