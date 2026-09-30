# 标准硬件新产品接入：实施边界与验证门槛

本文记录 STM32F105/EC800 标准 App 的内部迁移状态。对外 MQTT V4、各产品 Flash 配置和冻结 Boot 合同以各自现有规范为准；本文件不改变协议或发布身份。

## 当前源码快照（2026-09-30）

Platform 的可选组件已由审查基线的 19 个扩展为 21 个，新增 `cloud_service` 与 `log_ring`。`ABoxCloudService` 已在送餐车承担启动、关联状态、响应及周期心跳调度；底盘与清扫车因冻结故障邮箱前的 RAM 边界，采用同组件内的紧凑 bootstrap/TX tracker，并保留产品 TX 队列。三车都已使用公共 MQTT trial executor、Enrollment 身份/服务适配、OTA AT adapter、标准 App CMake 接线、公共 MQTT Topic 构造和公共日志环；送餐车的私有 TLS runtime 已由公共 `ABoxMqttRuntime` 取代。

公共 `ABoxCloudInternalV2Envelope` 现在校验既有内部 V2 请求结构并有界提取 requestId；底盘与清扫车已经迁入，产品继续校验参数、选择命令并编码原响应。底盘可选 product 字段的严格匹配与清扫车不检查该字段的旧行为分别保留。送餐车内部请求与双锁持久化幂等尚由产品处理，不能仅因字段相似改用此入口。Platform 38 项主机测试、三车产品测试及 ARM 构建通过，三车标准测试板均完成重新入网、独立请求、`sync_state` 与 Broker 踢线后的 Manifest → Heartbeat → 完整状态回归。三车公共日志环的 `get_log`/Cloud 回归亦有各产品内部证据；送餐车 1.1.36 的两次 ICCID 读数无效，1.1.37 延后后台 AT 请求后在独立查询与踢线后的重查中获得有效读数，踢线后的首次查询仍可能暂时无效。

仍需继续上收公共响应 payload/dispatch、维护事务与 ConfigStore 的完整协调、Enrollment/OTA 默认服务托管、单 owner service attach 和标准 App 发布全流程。现有发布器完成产物验证与整体替换，但三个候选均未替换正式 `dist/`。测试板证据不能代替真实车辆 CAN、执行器、双锁和机械反馈验收；Broker 踢线不能代替完整蜂窝断网或迟到 PUBACK 注入。

## 验证复用规则

公共组件的状态机、边界和故障注入由 Platform 主机测试验证一次；变更公共组件时，再运行受影响的公共测试和链接它的产品 ARM 构建。产品侧只对本次变动触及的接线、Profile/报文、Config codec、资源预算与安全策略做定向回归。标准测试板用于新硬件接线、首次集成以及主机无法覆盖的 EC800/网络行为；已有相同路径的实板记录可以引用，不为每次纯内部代码整理重复刷三车、重新 Enrollment 和踢线。扩大到另一产品前必须通过该产品差异的测试，不能把共享板的成功推断为实车 CAN、双锁或机械反馈成功。

巡防底盘的服务端静止/新鲜轮速预检是通信切换的产品安全策略，不是公共 MQTT 组件的测试门槛；无车辆 CAN 的标准板仍可验证 MQTT 连接、重连和 Cloud 生命周期。该板不能给出受预检保护的底盘配置切换实板成功结论，除非另有经过审查的隔离测试入口；不得伪造轮速或放宽正式门禁。

## 已落地的公共接线

- `abox_platform_attach_standard_port(<app> HARDWARE stm32f105_ec800_v1)` 明确选择标准硬件目标，并为 App 链接 `platform_port_stm32f105.c`。它提供现有三个产品重复实现的 tick、UART1 同步发送、Flash 擦写/临界区、EC800 电源引脚和上电时序。Flash 地址仍读取产品 `boot_cfg.h`；产品保留启动、特殊外设和安全策略。
- `abox_platform_attach_standard_app(<app> PRODUCT_CONFIG_DIR <dir> SCHEDULER BAREMETAL|FREERTOS)` 将标准板的组件集合与上述端口一次性链接，并继续执行板型/调度器合同检查。它是新 App 的构建接入入口，不代替 CubeMX 启动文件、产品外设或通信任务的运行 attach；后两者仍需后续阶段完成。
- `ABoxMqttReceipt` 位于现有 `abox::mqtt_ec800` target 内，跟踪单个在途发布的 64 位 transport operation。迟到或不匹配的事件不会完成当前发布；复位或取消使在途结果失败。`SUBMITTED` 仍是待完成状态，`CONFIRMED` 仅表示模组给出该 operation 的发布结果，不等于 Broker 消费或业务成功。
- `ABoxMqttEc800Bridge` 统一三车过去重复的 transport/receipt/ICCID/工作区接线与 ready 边沿事件。产品保留静态配置、缓冲区和业务回调；回调表以静态常量保存在 Flash，避免增加送餐车最小堆与故障邮箱前的 RAM 占用。请求若发生在 MQTT RX 回调占用 AT 门禁期间，ICCID 读操作延后至 transport 与 AT 空闲；工作区容量不足时立即归还 lease。三车 ARM 链接通过，送餐车 1.1.37 在标准板验证专属 Enrollment、独立查询、`sync_state` 及 Broker 踢线后的启动顺序。产品旧 `MQTT_Core_*` 入口暂留兼容，最终新 App 可直接装配 bridge。
- `ABoxMqttV4_BuildTopic()` 统一五个公共 Topic 的构造；三个产品的身份切换与 App 初始化已接入。内部维护 Topic 仍沿用各产品合同，例如清扫车的 `/zxwl/sweeper_vcu/...`，不能用公共 `/zxwl/abox/...` 覆盖。
- 巡防底盘与清扫车已让 Cloud TX 依据精确 operation 推进响应、bootstrap 和各自的 candidate proof。送餐车也已把 Manifest、状态、响应、试连受理及心跳证明关联到 operation 和 requestId；旧发布计数接口暂留兼容。送餐车 1.1.34 已迁入公共 `ABoxMqttRuntime`。
- `ABoxEc800Recovery` 对 AT 隔离后的模组电源轨重启做有界冷却判断，产品或 Cloud service 仍需声明 OTA、安全及维护事务是否允许重启。送餐车已接入此门禁，重启后刷新 Enrollment HTTP 和 OTA 网络准备；保持 AT 隔离直到物理 RDY，不能仅靠 MQTT 重连解除。
- `ABoxMqttTrialExecutor` 将 trial 状态机的六种动作统一排入单个通信 owner 队列；超时或取消后的 RESTORE 抢占尚未执行的旧动作，取动作时校验 session。产品仍决定持久化 codec、证明报文、安全准入和实际 transport 操作。三产品均已接入公共 executor，并在标准测试板分别完成其既有证明类型的正向试连；送餐车又验证了不存在的候选 Broker 超时后恢复旧认证连接和重启持久化。TLS、认证及掉电等负向组合仍需继续验证。
- `ABoxConfigStore_Commit` 是无 Flash 布局假设的配置提交门禁：产品回调负责候选和稳定快照的写入加回读，Platform 在候选失败时执行一次稳定快照恢复，并把已保存、已恢复和恢复失败分别返回。清扫车已迁入且保持原 Config V3 codec；同参数候选在标准板完成服务端事务、独立回读和 MCU 复位后重新上线。底盘源码也已迁入且保持 Config V5 codec/恢复页，主机测试与 ARM 构建通过，但标准板没有真实 CAN 轮速，服务端在安全预检阶段拒绝候选写入，故不能声称底盘实板提交成功。送餐车源码候选已接入同一提交门禁，保留 Config V2 的准备、写入和回读，主机注入覆盖保存、失败恢复及恢复失败，ARM Release 构建通过；尚未在标准板执行此候选的持久化事务，不能声称其实际掉电原子性已改变。
- 新增可选 `abox::cloud_service` 的 `ABoxCloudService` 调度核心：单 owner、连接 generation 与发布 operation 关联，统一 Manifest → Heartbeat → 完整 State 启动顺序，以及响应、关联状态报告、事件和周期上报的有界排队。产品仍构造原有 MQTT V4 报文、提供实际 transport、维护证明和业务状态。送餐车已迁移启动三报文、`sync_state` 关联状态报告、周期心跳及普通响应的调度与 operation 回执；在标准测试板确认响应先于同 requestId 报告、约 5 秒心跳、Broker kick 和同参数试连后重连恢复。响应 payload 队列、其他命令 dispatch、OTA 与 Cloud 主循环仍由产品实现。送餐车原协议没有周期完整状态，因此公共选项设为零以关闭该调度。底盘与清扫车已接入紧凑启动游标和 TX 跟踪器，完整服务对象仍受 RAM 边界约束。
- `ABoxCloudBootstrap` 是同一组件中的小型启动游标，提供相同的三报文顺序、连接 generation 和精确 operation 回执，不分配 request、response 或事件字符串。清扫车和巡防底盘的现有 TX scheduler 暂时继续管理其余报文，以满足固定故障邮箱前的 RAM 约束；迁移公共 Cloud 全功能时需替换产品重复状态，不能并存两份大型缓存。两个产品均在标准测试板重新入网，Broker 踢线后订阅采集到 Manifest → Heartbeat → 完整 State，并验证 `sync_state` 响应先于关联状态。
- `ABoxBootV2Ec800Adapter_Bind()` 位于既有 `abox::boot_v2_app`，统一 OTA owner 的 AT 提交、RAW、URC、取消与接收溢出计数。产品继续提供 tick、MQTT 暂停/停止和工作区、日志、版本、artifact、Flash 布局及传输缓冲区；App OTA 状态机仍是原有 `ABoxBootV2App`。
- Enrollment 的 TLS 凭据按产品声明的预期 profile 字符串校验；既有默认值为 1，底盘显式声明 0。profile 不匹配时拒绝进入候选试连，明文入网的现有行为不变。服务端仍按产品专属 hardwareContract 决定入网策略。
- `ABoxEc800Iccid` 在同一 AT owner 中查询、解析并复位 SIM 身份；三产品 adapter 已移除重复的 `AT+QCCID` 处理。`ABoxMqttRuntimeOptions_StandardEc800()` 集中标准板的 client、TLS context、CA 与超时参数，底盘和清扫车仅声明 owner/profile 及特殊测试超时。
- `ABoxEnrollmentIdentity_Build()` 统一 STM32 UID 与 fallback VID 的有界格式化，产品显式提供 prefix、已分配 VID 判据和 hardwareContract。`ABoxEnrollmentService` 托管 HTTPS/HTTP cleanup、MQTT 暂停、入网轮询与试连结果推进；产品留下网络就绪、身份、配置需求、试连及持久化 callback。三个产品均已接入，Flash codec 与产品身份未变。
- `tools/publish_release.py` 使用产品声明的 `release_contract.json` 校验冻结 Boot 哈希、App 向量与 Flash 上界、版本标记、原有 Manifest/产物哈希、Full 的 Boot+App 拼接，再复制到暂存目录复核并整体替换 `dist/`；失败时保留或恢复旧目录。产品仍负责构建、产品专属额外校验和现有 Manifest 字段。三个现有正式包已只读通过新合同；Platform 主机测试覆盖损坏产物拒绝与替换失败回滚，新的候选固件尚未借此正式发布。

三个产品的 ARM Release 构建通过。Platform GCC/Ninja 主机测试 36 项、底盘主机测试 39 项、清扫车 27 项以及送餐车协议、TLS runtime、adapter、Profile、Boot 合同测试通过。底盘既有 cp1/fi2/cp2 台架证据仍有效；禁用执行器的 cp5 以及接入公共 executor 的 0.2.48-cp1 在标准测试板按底盘 hardwareContract 完成 HTTPS 入网、MQTT 首连、Config V5 持久化及独立 `get_info`。清扫车 cp1 与接入公共 executor 的 1.3.7-cp1 也在同一标准板完成入网、试连证明及 Config V3 头部回读。送餐车 1.1.24 完成入网、明文 MQTT 候选心跳证明、Config V2 持久化及独立 `get_info`；1.1.25 在 Broker 主动断开导致 AT 隔离后，模组电源轨重启并恢复 MQTT 与 OTA ready；1.1.26 在原配置上执行同参数 `set_mqtt`，收到 202、心跳证明后 trial 状态到 COMMITTED，独立 `get_info` 返回 200。1.1.27 首次将启动三报文接入公共 Cloud 核心，空配置时暴露 NOLOAD trial executor 未初始化导致的 HardFault，已拒绝该候选；1.1.28 修复初始化顺序后重新入网 verified；1.1.29 的关联状态报告及失败候选恢复、1.1.30 的周期心跳与 Broker kick 后恢复、1.1.31 的公共 Topic 构造与响应/报告顺序、1.1.32 的公共响应跟踪均通过标准板验证。1.1.32 使用不存在的 OTA revision 验证快速错误响应及后续 ready，但未覆盖持续下载的资源占用。各仓的 `docs/operations/evidence/2026-09-29-platform-*` 保留证据。1.1.26 首次刷写后冻结 Boot 曾停在 pre-App Error_Handler，手动复位及后续复位成功；该偶发现象尚未归因，不能算冷启动稳定性通过。完整蜂窝断网、真实迟到 PUBACK、持续 OTA 资源占用及实车行为仍无对应实板证据。正式发布状态仍由各产品 `dist/` 与验收记录决定。

- `ABoxMqttRuntimePort_BindCore()` 将产品既有 `MQTT_Core` 接入公共 runtime，合并暂停、安全门禁、配置、task、模组复位、停止与连接状态八项重复接线。产品仍声明 CA/时间、网络变化、安全撤销和日志；静态接线表不增加 App RAM。底盘 cp3 已在标准板验证 MQTT READY、Broker 断线后的启动顺序与 `sync_state`；清扫车 cp3 也在标准板重新入网 verified、得到独立 `get_info` 200，并在 Broker 断线后保持启动顺序与关联 `sync_state`。此接口是迁移桥接，新 App 最终应由公共 transport/runtime 直接装配。

- 标准板 `ABoxStm32F105Ec800Identity_Build()` 在 Hardware Port 内读取 STM32 UID 与冻结 Boot 描述符，再调用公共 Enrollment 身份格式化器。产品仍声明已分配 VID 判据、fallback 前缀、ICCID 来源、hardwareContract 和 App 版本；没有把产品身份写入 Platform。三产品已迁移，主机、ARM 及标准测试板的擦除测试配置后重新入网均通过：各自专属合同到 verified，独立 `get_info` 返回 200 与 OTA ready。送餐车首次查询时 ICCID 尚未有效，稍后重查有效，证据保留两个读数。

- 公共 `ABoxMqttRuntime` 新增可选 TLS context 能力探测：启用时先观察模组 `AT+QSSLCFG=?` 对目标 context 的范围声明，再允许 TLS lease；不支持或查询失败直接拒绝，绝不退回明文。主机测试覆盖支持、不支持和 RDY 后重新探测。既有底盘/清扫车默认接线尚未启用该选项，因此现行行为不变；送餐车 1.1.34 已在此基础上迁入公共 runtime。

## 下一阶段入口

本轮又把清扫车 `1.3.7-cp2` 与巡防底盘 `0.2.48-cp2` 的启动三报文接入紧凑游标：两者产品主机测试与 Platform 32 项 CTest、ARM Release 构建通过；标准板分别按其 hardwareContract 重新入网，Broker 踢线后抓到 Manifest → Heartbeat → 完整状态，独立 `get_info` 和关联 `sync_state` 响应/报告通过。底盘台架构建禁用所有车辆输出和 CAN 诊断，清扫车没有连接车辆负载；这不是运动或供电验收。原始证据分别在产品仓 `docs/operations/evidence/2026-09-29-platform-cloud-bootstrap-*/index.json`。

1. 巡防底盘台架继续补完整蜂窝断网、字面模组 RDY、真实迟到 PUBACK、OTA 占用和 candidate 成功矩阵；已完成的 fault injection 和 Broker kick 分别记录，不能把它们等同于所有网络故障。送餐车 1.1.26 的一次 pre-App Boot 停机须继续查明，并做冷启动复现。
2. 清扫车已把状态报告证明接到 operation 机制，送餐车已把心跳证明接到 operation 机制；三产品现均使用公共 trial executor。送餐车已接入公共 `ABoxMqttRuntime`，没有抽取第二个竞争性 runtime。保留各自 proof 类型、响应码和业务状态。
3. 将 `ABoxCloudService` 从送餐车已验证的启动、关联状态、心跳和普通响应扩展到公共响应 payload 队列、事件调度和 Profile dispatch；底盘与清扫车已接入紧凑启动游标，其余公共编排需逐步替换产品状态。现有公共接口尚未接管 JSON 包络、命令幂等、维护和 App OTA。产品提供身份与 Profile、静态资源、状态 provider、command handler、Config codec、维护/OTA 安全 callback；App 不因组件数量增加而增加 EC800 owner 或通信任务。底盘与清扫车仅剩约 4 KiB RAM guard，迁移必须替换重复状态而非简单叠加一个服务对象。
4. 继续上收 Enrollment/OTA 默认适配与发布清单生成；公共发布校验和目录替换已落地，仍需在通过产品验收后的下一次正式发布中验证整条入口。旧产品先用 codec 保留字节 ABI；不能把三种 Flash 格式强转为同一结构。

标准目标只决定引脚、电平和端口时序。OUT 通道的负载语义、动作顺序、运动或开锁准入、健康判据仍由产品实现；不同 MCU/板卡需提供自己的 Hardware Contract，不得将 STM32F105 假设放入公共业务组件。
