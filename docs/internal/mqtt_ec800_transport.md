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

`ABoxMqttEc800Rx` 目前是待审、未接入的 Platform 实现。长度式 `+QMTRECV` 可交付包含 CR/LF 和二进制字节的 payload；旧式 JSON/引号拼帧须显式开启兼容模式。超长帧按已知长度丢弃，截断或超时后锁定解析器。只有 UART/AT 所有者确认旧数据排空或模组连接重置后，才可调用 `Reset` 重新接收。回调不得递归 Feed/Reset。

公共 transport 接入前需完成：AT、MQTT、OTA RAW 的唯一分流；连接、订阅、发布、重连与取消的操作代次；发布确认与迟到 URC 隔离；静态缓冲区借出/归还；三个产品的行为测试和 ARM 资源检查。清扫车 SSL context 1 由 HTTPS 入网及 OTA 使用，MQTT 使用 context 2；context 2 的模组能力和并行行为仍需实机确认。
