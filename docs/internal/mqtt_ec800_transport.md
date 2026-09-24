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

`ABoxMqttEc800_Publish` 返回的是排队成功及操作标识。`QMTPUBEX` 的 QoS0 `msgid` 必须为 0；QoS1 使用非零递增 ID。回调的 `SUBMITTED` 表示 payload 已交给模组，`CONFIRMED` 表示收到对应消息 ID 的模组结果；两者均不能证明 Broker 或业务消费端已接收。发布超时或无法安全取消活动 AT 命令时锁定 AT，必须物理复位并调用 AT 与 transport 的复位入口后复用。

旧式 `+QMTRECV` 的引号包裹 JSON 有两种实际形式：内部引号带反斜杠，或内部引号原样输出。兼容解析器在首字节为 `{`/`[` 时按第二字节区分，原样形式按 JSON 嵌套结束交付，避免仅把首个 `{` 送给产品协议解析器。发布返回失败时 transport 请求关闭重连；若关闭无法确认则进入 BLOCKED，产品需重启模组后才能继续使用 AT。

`ABoxMqttEc800_Stop` 只有在 QMTDISC、QMTCLOSE 和迟到 URC 排空窗口结束后返回 1；返回 -1 表示状态不确定。配置更新只允许在 IDLE 或 PAUSED，工作区只允许在 PAUSED 且 AT 无未完成命令时借出，归还时重置 RX。runtime 提供可选 `mqtt_stop` 回调；产品接入时须使用此回调把关闭权交给 transport，避免 runtime 和 transport 双方各发一套 QMTDISC/QMTCLOSE。

同时接入公共 runtime 和 transport 时，runtime 的 `mqtt_modem_reset` 回调须在收到 `RDY` 后于关闭安全门期间复位 transport，再重用保存的配置。否则 transport 可先进入 CONFIGURING，导致 runtime 的配置更新被拒绝并停在 FAILED。

模组上电后 `CEREG` 可暂时为搜索中。transport 在配置阶段未建立 MQTT 会话，失败时进入重试等待，不发送此时无效的 `QMTDISC`/`QMTCLOSE`；已建立会话后的关闭仍按原有确认流程。

`QMTOPEN` 的 AT `OK` 只确认命令受理。若随后收到同一 client 的 `+QMTOPEN: <client>,-1`，模组明确报告 socket 未打开；transport 进入迟到 URC 排空及重试等待，停止时可释放该 client，不向未建立的会话发送 `QMTDISC`/`QMTCLOSE`。其他打开失败或结果不确定时仍按原关闭及隔离流程处理。现场 TLS 候选曾在此失败后因两条无效关闭命令返回 `ERROR` 而误隔离 AT；修复后候选失败由 trial 恢复原连接并返回原请求的最终结果。

当前独立主机测试覆盖正常连接、订阅、接收、发布、重复确认、关闭排空、断线重连、订阅失败、借还及发布超时后的锁定与复位。三个产品尚未启用该组件；这不是设备模组、Broker 或业务平台验收证据。

App OTA 的可选回调 `mqtt_stop`、`workspace_borrow`、`workspace_return` 允许下载器先等待 transport 关闭并排空，再借用既有传输缓冲区，完成或失败时归还后恢复 MQTT。未接入回调的旧产品仍走原有关闭流程；冻结 Boot 不参与此 App 侧变更。
