# A-BOX 设备公共 MQTT 协议

文档修订：V3.0

报文版本：`3.0`

修订日期：2026-08-31

## 1. 协议定位

本文档只定义车载 A-BOX 公共业务可复用且可由平台观察、发送或校验的 MQTT 契约，包括报文外层、QoS、请求幂等、公共响应码、心跳和兼容性规则。设备内部轮询、总线通信、执行器拓扑、状态机和故障恢复算法不属于平台协议。

设备命令、状态字段和安全语义由对应产品仓库的业务 Profile 定义。当前已登记的公共设备类型为：

| `deviceType` | 业务 Profile |
|---|---|
| `wing_actuator` | A-BOX 飞翼设备业务协议 |
| `locker_compartment` | A-BOX 格口设备业务协议 |
| `vehicle_chassis` | A-BOX 车辆底盘业务协议 |

新增设备类型必须先登记稳定的 `deviceType`，再由对应产品仓库维护业务 Profile；不得在公共协议中加入某一设备独有的命令或状态字段。

设备信息、远程日志、VID/MQTT 配置和 Boot V2 OTA 属于受控内部运维接口，使用独立的 `/internal/*` 协议，不属于公共业务平台接口。厂家 CAN、RS485、执行器帧格式及设备内部调度也不属于本文档。

平台接入某类设备时，只需实现本文档和对应设备 Profile，不需要实现其他设备的 Topic 或命令。

## 2. 对接前置条件

平台应提供：

- MQTT Broker 地址和端口；
- MQTT 用户名和密码；
- 全平台唯一的 `deviceId`；
- 设备对应的固定 `deviceType`。

`deviceType` 表示设备能力和公共业务 Profile，不表示客户、购买方、车型或项目。项目归属由平台按 `deviceId` 在资产台账或运维备注中管理，不得为项目派生新的 `deviceType`。

`deviceId` 只允许字母、数字、下划线和连字符，按平台分配值原样写入 Topic，并在全部 A-BOX 设备中保持唯一。

## 3. 基础约定

1. 所有 MQTT Payload 均为 UTF-8 JSON。
2. 固定 Topic 段使用小写。
3. `timestamp` 为发送端生成的 UTC Unix Epoch 毫秒整数。
4. 所有公共请求、响应、心跳和状态报告的 `data` 都必须包含 `deviceType`。
5. 设备必须校验公共请求中的 `deviceType`；与固化值不一致时返回 `4004`，不得执行命令。
6. 平台必须忽略 `data` 中无法识别的兼容扩展字段。
7. 平台收到未知枚举时应保存原始值，并按 `unknown` 降级展示，不得丢弃整条报文。
8. 报文外层 `version` 固定为字符串 `"3.0"`。新增命令、设备 Profile 或可忽略字段可以继续使用 `3.0`；改变既有字段名称、类型、必填性或含义必须升级报文主版本。

## 4. MQTT QoS、Retain 与在线判定

| 消息类型 | QoS | Retain | 说明 |
|---|---:|---:|---|
| 平台业务请求 | 1 | 否 | 至少一次送达，设备按 `requestId` 幂等处理 |
| 设备业务响应 | 1 | 否 | 返回对应请求的处理结果 |
| 设备心跳 | 0 | 否 | 每 5 秒发送，允许少量丢失 |
| 设备离散状态或安全事件 | 1 | 由 Profile 定义 | 必须允许重复投递，平台按消息标识幂等处理 |
| 设备高频周期样本 | 0 | 由 Profile 定义 | 尽力上报，允许丢失和接收抖动 |

订阅端请求的 QoS 是交付上限。平台订阅响应和包含 QoS 1 事件的 Topic 时必须请求 QoS 1，避免把端到端交付等级降为 QoS 0。

A-BOX 每 5 秒发布一次心跳。平台连续 15 秒未收到心跳时判定 A-BOX 离线，并以 Broker 接收时间为准，不使用设备报文时间戳替代接收时间。

Retain 状态只代表最近一次业务状态，不证明设备当前在线。遗嘱消息只作为异常断线辅助通知，最终仍以心跳超时判定离线。A-BOX 在线和下游业务控制器在线必须分别展示。

## 5. 通用报文

### 5.1 消息外层

```json
{
  "version": "3.0",
  "timestamp": 1770186088333,
  "data": {
    "deviceType": "example_device"
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `version` | String | 是 | 固定为 `3.0` |
| `timestamp` | Integer | 是 | UTC Unix Epoch 毫秒时间戳 |
| `data` | Object | 是 | 业务数据对象 |
| `data.deviceType` | String | 是 | 设备固化的能力类型，用于选择业务 Profile |

### 5.2 平台请求

```json
{
  "version": "3.0",
  "timestamp": 1770186088333,
  "data": {
    "deviceType": "example_device",
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "command": "example_command",
    "params": null
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `data.deviceType` | String | 是 | 目标设备能力类型 |
| `data.requestId` | String | 是 | 平台生成的全局唯一请求标识 |
| `data.command` | String | 是 | 设备 Profile 定义的命令 |
| `data.params` | Object / null | 是 | 命令参数；无参数时为 `null` |

### 5.3 设备响应

```json
{
  "version": "3.0",
  "timestamp": 1770186088456,
  "data": {
    "deviceType": "example_device",
    "requestId": "3fa85f64-5717-4562-b3fc-2c96063f8666",
    "code": 200,
    "msg": "operation accepted"
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `data.deviceType` | String | 是 | 设备固化的能力类型 |
| `data.requestId` | String | 是 | 原样返回请求的 `requestId` |
| `data.code` | Integer | 是 | 公共码或设备 Profile 专用响应码 |
| `data.msg` | String | 是 | 面向展示和诊断的结果描述；业务判断以 `code` 为准 |

公共响应不增加无意义的 `result:null` 包装。需要机器处理的设备专用结果字段，由对应 Profile 直接定义在 `data` 下；`msg` 只用于展示和诊断，不得承载需要程序解析的列表或对象。

## 6. 公共响应码

| 响应码 | 说明 |
|---:|---|
| `200` | 请求处理成功；具体成功边界由设备 Profile 定义 |
| `400` | 已定义该码的设备业务执行失败 |
| `4001` | JSON、参数非法，或同一 `requestId` 对应不同请求内容 |
| `4002` | 当前设备类型不支持该命令 |
| `4003` | 业务资源或控制流程繁忙 |
| `4004` | 请求 `deviceType` 与设备固化类型不一致 |
| `500` | 响应无法在规定单包上限内完整生成 |
| `5001` | A-BOX 内部异常 |

设备专用响应码只能在对应 Profile 中解释。平台遇到未定义响应码时应保留原始报文，不得套用其他设备类型的含义。

## 7. 请求幂等

设备必须缓存最近请求的 `requestId`、请求摘要和完整响应：

- 相同 `requestId` 且内容相同：返回首次完整响应，不重复执行；
- 相同 `requestId` 但内容不同：返回 `4001`，不执行新请求；
- 去重窗口必须覆盖 MQTT 重连和平台常规重试；容量及持久化方式属于设备实现。

平台重试时必须沿用原 `requestId`。生成新的 `requestId` 表示新的业务请求。

## 8. Topic 边界

Topic 中的 `{deviceId}` 替换为设备唯一标识。

公共 Topic 使用 `/zxwl/abox/{deviceId}/` 前缀。心跳 Topic 固定为 `/zxwl/abox/{deviceId}/heartbeat`；控制、响应和设备状态 Topic 由对应设备 Profile 注册。

设备只订阅自身精确的请求 Topic。平台只实现对应设备 Profile 声明的 Topic，不得把查询、主动报告和其他设备命令全部套用到同一设备。

## 9. 心跳

Topic：`/zxwl/abox/{deviceId}/heartbeat`

方向：A-BOX → 平台

频率：5 秒

QoS：0

Retain：否

```json
{
  "version": "3.0",
  "timestamp": 1770186088333,
  "data": {
    "deviceType": "example_device",
    "firmware": "device-firmware-1.0.0"
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `data.deviceType` | String | 是 | 设备固化能力类型 |
| `data.firmware` | String | 是 | 当前运行的 App 固件版本 |

心跳只证明 A-BOX 与 Broker 的通信存活，不证明下游控制器、机械动作、定位或 OTA 正常。

## 10. 平台最小实现检查表

- 使用精确的设备 Profile 和固定 `deviceType`；
- 请求、响应和关键状态订阅使用正确 QoS；
- 请求重试复用 `requestId`；
- 业务响应按 `requestId` 关联；
- 主动报告按设备 Profile 定义的报告标识幂等；
- 在线状态只由心跳接收时间判定；
- Retain 状态明确标记为最新历史状态，不替代在线状态；
- 未知字段和枚举按兼容规则保存并降级展示；
- 公共业务平台不调用 `/internal/*` Topic。
