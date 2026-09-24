# EC800 MQTT transport 接入合同与现状差异

本文是内部实现记录，不属于 MQTT V4 对外协议。产品名称只用于核对接入差异，不参与 Platform 构建或运行时分派。

## 三产品现有行为

| 行为 | 巡防底盘 | 送餐车 | 清扫车 | 公共实现要求 |
| --- | --- | --- | --- | --- |
| 调度 | FreeRTOS | 裸机 | FreeRTOS | 由产品轮询，不创建任务 |
| MQTT RX 缓冲区 | topic 128、payload 1152、行 704 字节 | topic 128、payload 1281 字节；公开报文上限 1024、内部报文上限 1280 | topic 128、payload 1152、行 704 字节 | 缓冲区由产品提供；长度限制由产品回调判定，不内置 Topic 语义 |
| 接收交付 | UART 字节流和 AT 事件；跨行 JSON 收集，待处理消息单槽 | AT 事件；跨行 JSON 收集，内部大报文单槽 | UART 字节流和 AT 事件；跨行 JSON 收集，待处理消息单槽 | 仅 UART/AT 所有者分流一次；回调携带明确长度，溢出和超时拒绝 |
| 发布 | `QMTPUBEX`，单个在途发布，`pub_ok_seq` | 同左；应用层按响应、心跳、日志优先级调度 | 同巡防 | 返回提交结果；后续事件区分发送完成和 QoS1 确认，并携带操作标识 |
| TLS | 公共 runtime 管理，MQTT SSL context 2 | 现有产品 Core 内置 TLS 编排，待改由公共 runtime/trial 管理 | 公共 runtime 管理，MQTT SSL context 2 待实机确认 | transport 不持有证书策略或产品 Flash 布局 |
| OTA 共享资源 | 现有 MQTT 缓冲区可给 OTA 使用 | OTA RAW 传输另有静态缓冲区 | 现有 MQTT 缓冲区可给 OTA 使用 | 通信静止后显式借出，归还后清理 RX 状态；OTA RAW 不进入 MQTT 解析器 |

当前三个产品仍编译私有 MQTT Core。此表固定替换前的行为，不能作为已完成公共 transport 接入的证据。

## 接收边界

`ABoxMqttEc800Rx` 已作为独立 Platform 组件加入，但尚未由产品启用。长度式 `+QMTRECV` 可交付包含 CR/LF 和二进制字节的 payload；旧式 JSON/引号拼帧须显式开启兼容模式。`ABoxEc800At_SetMqttReceiver` 在 AT 字节入口识别 MQTT URC，将完整报文交给该解析器；OTA RAW 所有权优先。超长帧按已知长度丢弃，截断或超时后锁定解析器并隔离 AT。只有 UART/AT 所有者确认旧数据排空或模组连接重置后，才可调用 `Reset` 重新接收。回调不得递归 Feed/Reset。

公共 transport 接入前需完成：将唯一分流点接入三个产品；连接、订阅、发布、重连与取消的操作代次；发布确认与迟到 URC 隔离；静态缓冲区借出/归还；三个产品的行为测试和 ARM 资源检查。清扫车 SSL context 1 由 HTTPS 入网及 OTA 使用，MQTT 使用 context 2；context 2 的模组能力和并行行为仍需实机确认。

## 公共 transport 接口

`abox::mqtt_ec800` 使用调用方静态缓冲区和配置字符串，不分配堆内存，不生成产品 Topic。AT owner 必须为 MQTT；`ABoxEc800At_SetMqttReceiver` 在唯一 AT 接收入口将 `+QMTRECV` 转给公共解析器，OTA RAW 仍由 AT 层优先处理。正常模式只按长度交付；旧式 JSON 拼帧由 `legacy_json` 显式开启。

`ABoxMqttEc800_Publish` 返回的是排队成功及操作标识，回调的 `SUBMITTED` 表示 payload 已交给模组，`CONFIRMED` 才表示收到对应消息 ID 的模组结果；两者均不能证明 Broker 或业务消费端已接收。发布超时或无法安全取消活动 AT 命令时锁定 AT，必须物理复位并调用 AT 与 transport 的复位入口后复用。

`ABoxMqttEc800_Stop` 只有在 QMTDISC、QMTCLOSE 和迟到 URC 排空窗口结束后返回 1；返回 -1 表示状态不确定。配置更新只允许在 IDLE 或 PAUSED，工作区只允许在 PAUSED 且 AT 无未完成命令时借出，归还时重置 RX。runtime 提供可选 `mqtt_stop` 回调；产品接入时须使用此回调把关闭权交给 transport，避免 runtime 和 transport 双方各发一套 QMTDISC/QMTCLOSE。

当前独立主机测试覆盖正常连接、订阅、接收、发布、重复确认、关闭排空、断线重连、订阅失败、借还及发布超时后的锁定与复位。三个产品尚未启用该组件；这不是设备模组、Broker 或业务平台验收证据。
