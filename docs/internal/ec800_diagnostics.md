# EC800 SIM / 网络诊断（内部）

`ABoxEc800Diagnostics` 是 opt-in 的 UART owner 内组件；未接入产品没有诊断 AT 或行为变化。
清扫车使用现有 AT core 的第六个 handler（owner PRODUCT_BASE+6）。不创建通信管理器、
MQTT 状态机、任务、堆分配、GPIO 驱动或模组重启策略。

复位后空闲时一次 `AT+CMEE=1`，随后 `AT+CPIN?`；确认 READY 才查询 `AT+CEREG?`。
CPIN 缺卡仅接受 CME 10 / SIM not inserted；PIN/PUK 对应 CPIN SIM PIN/SIM PUK 或
CME 11/12，其他明确 SIM 错误仅映射 CME 13/15。NOT READY、通用 ERROR、QCCID 失败、
CEREG 搜网、未知错误码都不能建立缺卡状态。标准错误码语义参考
[移远官方 AT 手册](https://quectel.com/content/uploads/2021/03/Quectel_EC25EC21_AT_Commands_Manual_V1.3.pdf)；
当前 EC800 固件具体返回格式仍以实板记录为准。没有设置 QSIMDET/QSIMSTAT，也不假定热插拔可恢复。

注册状态只接受当前诊断请求的有效 CEREG 查询响应，1/5 表示本地/漫游注册；
CGATT、QIACT 沿用既有 MQTT transport 结果，不重复建立 PDP。MQTT_READY 使用 transport
完成订阅后的 Ready。快照保留状态、valid、最后 CME/AT result、注册状态和采样 tick。
诊断/快照读取在同一 UART owner 任务中串行执行；跨任务产品应自行保护快照复制。

只有产品允许窗口且整个 AT 队列空闲才 NORMAL 提交，单命令超时 3000 ms；
检测周期/异常重试均 15000 ms，快照 60000 ms 过期。队列、RAW、隔离及发布接收门禁
继续由唯一 AT core 执行，不抢占任何操作。超时显示 MODEM_NO_RESPONSE，不等同缺卡。
RDY/物理重置清除有效性，并重新执行 CMEE/CPIN。诊断本身从不请求重启模组。

`abox_ec800_diagnostics_test` 覆盖缺卡、PIN/PUK、SIM failure、未知返回、注册/PDP/Ready、
AT 超时、恢复、过期、复位、时间回绕和已有高优先级 owner 排队。板载灯时序归 board_io。
