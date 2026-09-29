# ABox STM32F105 板载 I/O

`abox::board_io` 是不依赖 HAL、FreeRTOS 的公共通道状态组件；
`targets/stm32f105_ec800_v1/board_io_stm32f105.c` 是当前板型的 GPIO 适配。
产品通过 `abox_platform_attach_components(... HARDWARE stm32f105_ec800_v1 COMPONENTS board_io ...)`
显式链接，其他板型可用同一接口提供自己的适配。公共 Boot 不使用该组件。

硬件依据是仓内 `docs/internal/hardware/SCH_AirprotTbox_2026-06-15.pdf`，
原始来源为 2026-06-15 AirportTbox V1.0 原理图，SHA256：
`FF94C39B87A9D606FF776CB1DF4A3F963A85156DFE1A51191B3D5D0F8DB95209`。
输入通道是光耦隔离；外部通断使光耦导通后 MCU 侧读高电平。

| 通道 | MCU | 通道 | MCU |
| --- | --- | --- | --- |
| OUT1 | PA0 | IN1 | PC3 |
| OUT2 | PA1 | IN2 | PC2 |
| OUT3 | PA2 | IN3 | PC1 |
| RELAY | PC4 | IN4 | PC0 |
| LED1 | PB0 | IN5 | PA7 |
| LED2 | PB1 | IN6 | PA6 |
| LED3 | PB2 | IN7 | PA5 |
|  |  | IN8 | PA4 |

所有输出、LED 均高电平有效。初始化先写低电平，再将输出引脚配置为推挽输出；
其他产品不得由此推断它们的负载或联锁。`allowed_outputs` 位掩码只允许产品
显式开启的通道；未授权通道仍可执行关闭。输出写入会核对 MCU ODR 锁存位，
读回失败时尝试拉低，不能把这一读回解释为真实负载或继电器触点反馈。

输入每次 `ABoxBoardIo_Poll(now_ms)` 采样，稳定 20 ms 后设置有效位。
读取失败清除对应有效位，恢复后重新计时。`raw_mask` 和 `stable_mask`
只有在对应 `valid_mask` 位为 1 时才能用于业务安全判断。
LED 常亮、关闭或闪烁也由 `Poll` 推进，不在公共组件中定义业务灯义。
产品需在 10 ms 左右的既有周期调用它；不用新任务或阻塞延时。

平台主机测试和 HAL 桩只验证映射与软件行为。电源负载、触点和光耦电气表现
必须在实板上另行记录，不能用 MCU 锁存位或主机测试代替。
