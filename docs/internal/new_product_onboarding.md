# 标准硬件新产品接入：实施边界与验证门槛

本文记录 STM32F105/EC800 标准 App 的内部迁移状态。对外 MQTT V4、各产品 Flash 配置和冻结 Boot 合同以各自现有规范为准；本文件不改变协议或发布身份。

## 已落地的公共接线

- `abox_platform_attach_standard_port(<app> HARDWARE stm32f105_ec800_v1)` 明确选择标准硬件目标，并为 App 链接 `platform_port_stm32f105.c`。它提供现有三个产品重复实现的 tick、UART1 同步发送、Flash 擦写/临界区、EC800 电源引脚和上电时序。Flash 地址仍读取产品 `boot_cfg.h`；产品保留启动、特殊外设和安全策略。
- `ABoxMqttReceipt` 位于现有 `abox::mqtt_ec800` target 内，跟踪单个在途发布的 64 位 transport operation。迟到或不匹配的事件不会完成当前发布；复位或取消使在途结果失败。`SUBMITTED` 仍是待完成状态，`CONFIRMED` 仅表示模组给出该 operation 的发布结果，不等于 Broker 消费或业务成功。
- 巡防底盘与清扫车已让 Cloud TX 依据精确 operation 推进响应、bootstrap 和各自的 candidate proof。送餐车源码候选也已把 Manifest、状态、响应、试连受理及心跳证明关联到 operation 和 requestId；旧发布计数接口暂留兼容。送餐车仍使用私有 TLS runtime，尚未迁入公共 `mqtt_runtime`，也未对这个候选做实板回归。
- `ABoxBootV2Ec800Adapter_Bind()` 位于既有 `abox::boot_v2_app`，统一 OTA owner 的 AT 提交、RAW、URC、取消与接收溢出计数。产品继续提供 tick、MQTT 暂停/停止和工作区、日志、版本、artifact、Flash 布局及传输缓冲区；App OTA 状态机仍是原有 `ABoxBootV2App`。
- Enrollment 的 TLS 凭据按产品声明的预期 profile 字符串校验；既有默认值为 1，底盘显式声明 0。profile 不匹配时拒绝进入候选试连，明文入网的现有行为不变。服务端仍按产品专属 hardwareContract 决定入网策略。
- `ABoxEc800Iccid` 在同一 AT owner 中查询、解析并复位 SIM 身份；三产品 adapter 已移除重复的 `AT+QCCID` 处理。`ABoxMqttRuntimeOptions_StandardEc800()` 集中标准板的 client、TLS context、CA 与超时参数，底盘和清扫车仅声明 owner/profile 及特殊测试超时。
- `ABoxEnrollmentIdentity_Build()` 统一 STM32 UID 与 fallback VID 的有界格式化，产品显式提供 prefix、已分配 VID 判据和 hardwareContract。`ABoxEnrollmentService` 托管 HTTPS/HTTP cleanup、MQTT 暂停、入网轮询与试连结果推进；产品留下网络就绪、身份、配置需求、试连及持久化 callback。三个产品均已接入，Flash codec 与产品身份未变。

三个产品的 ARM Release 构建通过。Platform GCC/Ninja 主机测试 28 项、底盘主机测试 39 项、清扫车 27 项以及送餐车协议、TLS runtime、adapter、Profile、Boot 合同测试通过。底盘既有 cp1/fi2/cp2 台架证据仍有效；新增禁用执行器的 cp5 在标准测试板上擦除 Config 页后，按底盘 hardwareContract 发起 HTTPS 入网，服务器请求经测试板批准到 `verified`，MQTT 首连和 Config V5 400 字节持久化完成，独立 `get_info` 返回 200、READY、ICCID 与 OTA ready。测试板现运行 cp5；这轮入网已轮换其测试凭据。清扫车和送餐车本轮源码候选尚未上板。字面 `RDY`、完整蜂窝断网、真实迟到 PUBACK、OTA 占用与实车行为仍无新实板证据；见底盘仓 `docs/operations/evidence/2026-09-29-platform-enrollment-service/`。正式发布状态仍由各产品 `dist/` 与验收记录决定。

## 下一阶段入口

1. 巡防底盘台架继续补完整蜂窝断网、字面模组 RDY、真实迟到 PUBACK、OTA 占用和 candidate 成功矩阵；已完成的 fault injection 和 Broker kick 分别记录，不能把它们等同于所有网络故障。
2. 清扫车已把状态报告证明接到 operation 机制，送餐车候选已把心跳证明接到 operation 机制；下一门槛分别是 App 实板验证。送餐车私有 TLS runtime 仍待接入已有公共 `mqtt_runtime`，避免抽取第二个竞争性 runtime。保留各自 proof 类型、响应码和业务状态。
3. 在公共 runtime/transport 接线稳定后，新增 `ABoxCloudService` 组合层：产品提供身份与 Profile、静态资源、状态 provider、command handler、Config codec、维护/OTA 安全 callback；平台托管公共包络、request token、bootstrap、`sync_state`、维护事务及 OTA 响应协调。App 不因组件数量增加而增加 EC800 owner 或通信任务。
4. Enrollment/OTA 默认适配和统一发布引擎须作为独立提交，并分别验证现有协议、Boot/Flash 布局、配置持久化及失败回滚。旧产品先用 codec 保留字节 ABI；不能把三种 Flash 格式强转为同一结构。

标准目标只决定引脚、电平和端口时序。OUT 通道的负载语义、动作顺序、运动或开锁准入、健康判据仍由产品实现；不同 MCU/板卡需提供自己的 Hardware Contract，不得将 STM32F105 假设放入公共业务组件。
