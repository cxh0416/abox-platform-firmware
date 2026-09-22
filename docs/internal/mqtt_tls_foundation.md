# MQTT TLS 公共基础：阶段 1 接口与测试

本文为内部实现文档，不属于对外 MQTT 协议。基于独立平台工作区
`f2fec93` 增量实现；不更新产品中的平台快照。

## 当前交付边界

四个独立 C11 静态库、实例式接口、异步端口合同和主机单元测试。
无 HAL、RTOS、产品头文件、动态内存、Topic、心跳、Flash 布局依赖。
新库必须显式链接，不改变 `abox_platform_attach*` 的现有依赖。
现有 OTA、HTTPS 下载器、冻结 Boot 和产品固件不迁移、不重打包。

本阶段不是完整模组驱动迁移或实机 MQTT TLS 放行：

- UFS 已实现请求校验、路径副本、操作期限、长度检查、取消及隔离；
  `QFUPL/QFOPEN/QFREAD/QFCLOSE`、raw 传输、回读比较由后续 UFS 端口实现。
- TLS 已实现租约管理和五步 `QSSLCFG` 序列；CA 来源可信性、UFS 内容
  回读和有效时钟由集成方验证，再传入 `ca_verified/time_valid`。
  这两个标志不是平台自行完成证书验证的证据。
- MQTT TLS 已实现 SSL 绑定/解除；MQTT open/connect/subscribe/publish 仍由调用方负责。
- trial 已实现候选生命周期；连接、指定证明消息、持久化回读及恢复是异步产品回调。

## 文件与依赖

| 库 | 公共接口 | 实现 | 直接依赖 |
|---|---|---|---|
| `abox::ec800_ufs` | `include/abox_ec800_ufs.h` | `components/ec800/abox_ec800_ufs.c` | UFS 异步端口 |
| `abox::ec800_tls` | `include/abox_ec800_tls.h` | `components/ec800/abox_ec800_tls.c` | `ec800_ufs` 的路径校验、命令端口 |
| `abox::mqtt_tls` | `include/abox_mqtt_tls.h` | `components/mqtt/abox_mqtt_tls.c` | `ec800_tls`、命令端口 |
| `abox::mqtt_trial` | `include/abox_mqtt_trial.h` | `components/mqtt/abox_mqtt_trial.c` | 调用方异步回调 |

共用端口合同位于 `include/abox_ec800_async.h`。命令端口未来适配
`abox_ec800_at`；阶段 1 初始实现没有直接链接它。
测试为 `tests/abox_{ec800_ufs,ec800_tls,mqtt_tls,mqtt_trial}_test.c`，
命令模拟器为 `tests/ec800_async_fixture.h`；注册入口是根 `CMakeLists.txt`。

```text
mqtt_trial --> 调用方 prepare/connect/verify/commit/restore
mqtt_tls --> ec800_tls --> ec800_ufs（路径校验）
    |            |             |
    +-------- 命令端口        UFS 端口
                 \            /
                后续统一适配 ec800_at

现有 OTA --> 原有 HTTPS/UFS/AT 路径（本阶段保持原样）
```

## SSL context 规则

1. 每个物理模组唯一一个 `ABoxEc800Tls`，只在首次使用前 Init。
   所有 API 在同一事件循环中调用，或由调用方统一加锁，不能多任务无锁调用。
2. **context 1 永久保留给旧 HTTPS OTA**。现有 `abox_boot_v2_app` 和
   `abox_https_ufs_downloader` 仍配置它。管理器无论收到什么 supported mask，
   都拒绝申请 1；复位不会解除保留。
3. supported mask 必须来自确切模组型号/固件的验证。8 个槽仅为软件表容量。
   不默认宣称 context 2 可用；设备只有 context 1 可用时，新的 MQTT TLS
   申请失败，不退回共用 context 1，也不自动降级明文。
4. 租约包含 context 和单调 64 位 generation；owner 仅为诊断标识。
   同一 owner 再次申请也返回失败；旧租约不能释放新租约。
5. Prepare 仅接受新租约。准备完成后配置不可原地修改；CA/profile 轮换需要
   新租约或先解除旧绑定、释放租约后重新申请。使用期内 CA 文件内容不可改变。
6. MQTT Start(TLS) 先 pin 再发送绑定命令。MQTT 长连接期间一直持有 pin。
   绑定失败/超时仍保留 pin，因为命令可能已经在模组生效。
   **确认 MQTT 断开后**调用 Unbind，只有 `SSL=0` 成功才 unpin。
   lease 释放由调用方显式执行。Init 不能用作运行期清理。
7. Prepare 正常序列为 TLS 1.2、服务端校验、SNI、开启时间校验、指定 CA 文件。
   对应模组命令支持和主机名校验语义仍需实机确认；SNI 本身不证明主机名校验。
8. 未排空的取消进入 QUARANTINED，拒绝复用，直到实际模组复位和传输层排空。
   按顺序重置端口、TLS 管理器和 MQTT/UFS 实例；旧租约全部失效。

这种保留策略保护**通过新接口发起的 MQTT TLS**不会重配旧 OTA context。
它不能拦截绕过管理器的任意 AT 字符串。未来其他 TLS 使用者也必须纳入同一
资源清单；旧 OTA context 1 的不同 HTTPS 使用者之间仍沿用现有互斥机制。

## 异步端口与 UFS 合同

端口 submit/start 返回是否受理；poll 只查询对应 operation，不消费其他事务或 URC。
operation 由共享端口产生，存活期间不能复用。命令 submit 必须复制字符串，
不能保存调用栈上的地址。队列满时返回 0，TLS/MQTT 会在整体超时前重试。

cancel 返回 1 表示已停止操作且旧响应已排空。返回 0 时请求及借用缓冲区继续保留，
直到物理复位并排空。当前 `ABoxEc800At_Cancel` 本身不能证明迟到的普通 `OK`
不会命中新事务，因此后续适配器不得简单将其包装为 cancel 总是成功。
需要在公共层实现排空/恢复策略，并验证普通响应、URC、payload/raw 的交错。

UFS Start 会复制路径和请求描述，数据缓冲区由调用方保持有效；上传数据不可变。
READ/UPLOAD 的 OK 要求传输精确长度且句柄已经关闭；回读内容一致性检查由
调用方执行。REMOVE 的 OK 要求删除已完成。路径仅允许 `UFS:` 后的 ASCII
字母、数字、下划线、短横线、点，避免 AT 参数注入。
当前 API 不提供流式读写句柄；后续端口可分块实现完整请求。

CA 使用版本化专用文件，不覆盖 `UFS:ota_ca.pem`。正在使用的 CA 文件不得删除；
跨使用者 CA 引用计数、文件管理及受信更新链属于后续迁移工作。

## candidate 试连合同

```text
WAIT_ACCEPT -> PREPARING -> CONNECTING -> SUBSCRIBING -> VERIFYING -> COMMITTING -> COMMITTED
     任一失败/取消/超时 -> RESTORING -> FAILED 或 RESTORE_FAILED
```

- active/candidate 为调用方持有的不同只读对象；未提交前 active 指针不改变。
- ACCEPTED 只在旧连接上指定受理响应完成后上报。
- CONNECTED 后必须独立完成本产品所需订阅并上报 SUBSCRIBED，不能把连接成功
  当作已具备 proof 通道。
- 所有事件关联本轮 session；PROVED 还要匹配指定 proof ID。
  产品负责将 client、连接代次、指定消息的结果映射到这两个 ID。
  任意 publish 计数增加和通用 MQTT ACK 都不能自动成为证明事件。
- SAVED 必须意味着产品存储提交完成且回读通过；回调被受理不算保存成功。
- 恢复回调必须先停止 candidate 和未完成提交，恢复 active 持久化/运行状态后
  才能上报 RESTORED。若无法撤销在途保存，必须上报恢复失败。
  公共模块不承诺 Flash 原子性或掉电安全。
- 回调只提交工作，不重入状态机。事件带到达时间，超时边界优先于成功事件。
- 恢复有独立超时；RESTORE_FAILED 阻止新试连，只有显式确认外部恢复完成
  才允许 RecoveryComplete。原始失败原因保留在 reason 中。
- modem 复位由产品取消 trial 并进入恢复，不能清空实例后继续接收旧事件。

## 验证与复现

主机构建：

```powershell
cmake -S . -B build-mqtt-foundation -G Ninja -DCMAKE_C_COMPILER=D:/MinGW/bin/gcc.exe -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-mqtt-foundation
ctest --test-dir build-mqtt-foundation --output-on-failure
```

测试覆盖：context 1 保留、相同/不同 owner 冲突、租约代次、pin 与不可重配、
五步配置逐步失败、队列忙、两个 context 共用串行端口、取消与复位隔离、
UFS 路径/长度/缓冲区描述、32 位时钟回绕、显式明文解绑、绑定失败保留引用、
试连受理/会话/证明/提交门禁、保存失败恢复、恢复失败阻止新试连。
原有 8 个测试一并回归；这些测试不能替代新端口的 raw/URC 模拟或模组实测。

2026-09-21 本次验证结果：

- GCC 7.3 / Ninja Debug 全平台主机构建成功，CTest **12/12 通过**（新增 4，原有 8）。
- 四个新模块分别通过主机 `-std=c11 -Wall -Wextra -Werror -pedantic` 检查。
- 四个新模块分别通过 Arm GNU 14.2 的 Cortex-M3/Thumb freestanding 对象编译，
  同样启用上述严格告警；不是产品完整固件链接或硬件验收。
- 编译记录：`out/logs/mqtt-foundation-compile.txt`；测试记录：
  `out/logs/mqtt-foundation-ctest.txt`。ARM 对象位于 `out/mqtt-foundation-arm/`。
- OTA/HTTPS/Boot 源码与原有 attach 配置无 diff；没有产品固件或发布包变化。

## 后续迁移点（按顺序）

1. 实现公共 AT 命令端口及 UFS QF* 后端，补 raw、URC、迟到 OK、超时后排空测试。
2. 接入可信 CA 上传/逐字节回读、时钟验证、版本化 CA 引用管理；
   在真实 EC800 固件确认 context 数量、TLS 参数、主机名和时间校验能力。
3. 独立验证 MQTT TLS 与原 context 1 HTTPS OTA 并发；保持旧 OTA 序列做对照。
4. 再逐步迁移 OTA 的 UFS/TLS 重复实现，单独审核行为等价性。
5. 送餐车通过产品薄适配层接入，保留 Topic/心跳/业务/配置存储；本阶段未修改。
6. 底盘车另行设计 V4/V5 读取、candidate 提交、恢复和运动安全门禁；本阶段未修改。

只有完成端口和实机证据后才可将完整阶段 1 的设备侧验收判定为通过。

## 阶段 2 runtime 接入补充（2026-09-21）

新增 `include/abox_ec800_command_port.h`、
`components/ec800/abox_ec800_command_port.c` 和对应测试，提供对已有
`abox_ec800_at` 的实际异步命令适配，不增加串口执行器。CMake 目标为
`abox::ec800_command_port`，独立平台 CTest 增为 13/13 通过。

AT 层新增显式 quarantine 门禁：新适配器的活动事务取消、超时或发送失败
可能遗留未标记响应，因此隔离整个 AT 流并清除排队命令，拒绝后续发送；
只有确认物理模组复位后才能 Reset。未调用该门禁的既有 OTA 路径保持原样。
排队且未发送的请求可直接取消。新增测试以真实 AT Feed/Task 验证 URC、
迟到 OK、跨 owner 排队和发送失败，不仅使用模拟命令端口。

送餐车工作区只接入公共 TLS runtime，尚未接入公共 mqtt_trial。实机 context 2
配置及绑定通过，server196 的证书/入口条件阻塞 TLS 端到端验收。详见产品
`docs/operations/mqtt-tls-runtime-stage2.md`；不能据此宣称 TLS 上线。

## Platform runtime 配置合同补充（2026-09-22）

`abox_mqtt_trial` 的 active/candidate 已收敛为公共 `ABoxMqttConfig`，字段包含
host、port、username、password、`tls_enabled`、`tls_profile_id`；产品必须从各自
Flash ABI 映射，平台不依赖产品 layout。连接、订阅和指定 proof 现为三个独立门禁。
完整配置语义、四类切换、禁止 TLS 失败后隐式降级以及统一内部运维字段见
[`mqtt_runtime_contract.md`](mqtt_runtime_contract.md)。
