# ABox 公共入网实现

`ABoxEnrollment` 持有申请、轮询、凭据校验和试连结果状态；产品提供 UID、VID、产品
hardwareContract、Boot/App 标识、HTTPS origin、就绪门禁和配置持久化适配。URL、请求体、
响应、requestId 和 pollToken 缓冲区均由产品持有。模块不写 Flash，也不持有产品配置 ABI。

`ABoxEnrollmentEc800Http` 执行 EC800 HTTPS POST，并在 `QHTTPSTOP` 成功后才回调入网
核心。产品先确认蜂窝、CA、OTA 互斥和 AT 空闲，再调用 `Post`。HTTP 清理失败会阻止
新 HTTPS 操作；只有物理 modem reset 后才能调用 `AfterModemReset` 恢复。响应长度超过
调用方缓冲区时拒绝解析。

入网核心保留 pending requestId/pollToken。202 使用合法 `retryAfterSec`，缺失或无效时
等待 120 秒。401/403/404/410 清除 request 凭据并等待 120 秒重新申请；409、网络和
服务端暂时错误保留阶段。试连失败保留 request，下一次优先 poll 重新领取凭据；领取
窗口关闭后再次申请。取消时等待 HTTP 或试连适配完成清理。

MQTT 试连共用 `ABoxMqttTrial`。已有配置切换使用 `Start` 并等待旧连接响应 ACK；首次
入网使用 `StartFirst`，不需要旧配置或旧 ACK，120 秒试连超时、90 秒清理恢复超时。
`SAVED` 只能在 Flash 保存并读回后发送。首次试连恢复回调接收 `NULL` active config，
产品需清理候选连接、恢复入网前身份并保持普通 MQTT 关闭。恢复失败会阻止后续试连，
直到产品显式完成恢复。

主机测试覆盖请求复用、状态码、计时、响应边界、HTTP 清理门禁、首次试连以及恢复
阻塞。它们不证明蜂窝网络、Broker、Flash 断电或产品业务数据已通过现场验收。
