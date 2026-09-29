# 标准硬件新产品接入：实施边界与验证门槛

本文记录 STM32F105/EC800 标准 App 的内部迁移状态。对外 MQTT V4、各产品 Flash 配置和冻结 Boot 合同以各自现有规范为准；本文件不改变协议或发布身份。

## 已落地的公共接线

- `abox_platform_attach_standard_port(<app> HARDWARE stm32f105_ec800_v1)` 明确选择标准硬件目标，并为 App 链接 `platform_port_stm32f105.c`。它提供现有三个产品重复实现的 tick、UART1 同步发送、Flash 擦写/临界区、EC800 电源引脚和上电时序。Flash 地址仍读取产品 `boot_cfg.h`；产品保留启动、特殊外设和安全策略。
- `ABoxMqttReceipt` 位于现有 `abox::mqtt_ec800` target 内，跟踪单个在途发布的 64 位 transport operation。迟到或不匹配的事件不会完成当前发布；复位或取消使在途结果失败。`SUBMITTED` 仍是待完成状态，`CONFIRMED` 仅表示模组给出该 operation 的发布结果，不等于 Broker 消费或业务成功。
- 巡防底盘已让 Cloud TX 依据精确 operation 推进响应、bootstrap 和 candidate heartbeat proof。旧发布计数接口暂留供兼容调用；清扫车和送餐车尚未切换到 receipt。

公共 Port 已在三产品源码接入；三个产品的 ARM Release 构建通过。Platform GCC/Ninja 主机测试 24 项、底盘主机测试 39 项、清扫车 27 项以及送餐车既有协议/适配测试通过。底盘禁用执行器候选随后在标准测试板上完成启动、MQTT READY、Manifest 与状态落库、`get_info`、`sync_state` 和 MCU 重启重连；配置页保持原哈希，测试板已恢复原 App/State。此验证仍不覆盖独立 EC800 RDY、断网、迟到 PUBACK、候选失败、OTA 占用或实车行为；详见底盘仓 `docs/operations/evidence/2026-09-29-cloud-port-stage1/`。正式产品发布状态由各产品 `dist/` 和验收记录决定。

## 下一阶段入口

1. 在巡防底盘测试板按原硬件合同完成连接、断网、模组 RDY、迟到 PUBACK、candidate 成功/失败及 OTA 占用，记录 operation 与连接代次。确认 Cloud 的响应、心跳和恢复证明只由对应发布推进。
2. 通过上述门槛后，把清扫车状态报告证明和送餐车 heartbeat 证明分别接到同一 operation 机制；保留各自 proof 类型、响应码和业务状态。送餐车先接公共 `mqtt_runtime`，避免抽取第二个 TLS runtime。
3. 在公共 runtime/transport 接线稳定后，新增 `ABoxCloudService` 组合层：产品提供身份与 Profile、静态资源、状态 provider、command handler、Config codec、维护/OTA 安全 callback；平台托管公共包络、request token、bootstrap、`sync_state`、维护事务及 OTA 响应协调。App 不因组件数量增加而增加 EC800 owner 或通信任务。
4. Enrollment/OTA 默认适配和统一发布引擎须作为独立提交，并分别验证现有协议、Boot/Flash 布局、配置持久化及失败回滚。旧产品先用 codec 保留字节 ABI；不能把三种 Flash 格式强转为同一结构。

标准目标只决定引脚、电平和端口时序。OUT 通道的负载语义、动作顺序、运动或开锁准入、健康判据仍由产品实现；不同 MCU/板卡需提供自己的 Hardware Contract，不得将 STM32F105 假设放入公共业务组件。
