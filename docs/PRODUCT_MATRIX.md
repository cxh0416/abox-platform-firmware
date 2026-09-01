# A-Box 产品接入矩阵

## 当前状态

| 产品 | MCU/封装 | 运行方式 | 构建基线 | OTA/Boot 状态 | 产品专属边界 |
|---|---|---|---|---|---|
| `Top_Flying_Wing` | STM32F105R / LQFP64 | 裸机 | CMake | 已使用公共 Boot V2，App 下载 | 顶飞翼 CAN 与安全停止状态机 |
| `LockCtrlBoard_cheweishi` | STM32F105R / LQFP64 | 裸机 | CMake | 已使用公共 Boot V2，App 下载 | 锁板 RS485 与温度总线 |
| `Meal_Delivery_Vehicle` | STM32F105R / LQFP64 | 裸机 | CMake | 已使用公共 Boot V2，App 下载 | 双锁 RS485/MQTT |
| `Sweeper_VCU` | STM32F105R / LQFP64 | FreeRTOS | CMake | 已使用公共 Boot V2，App 下载 | VCU 任务、IWDG、双 CAN |
| `Airport_vehicle` | STM32F105R / LQFP64 | 裸机 | CMake | 旧 Boot 通过 HTTP 下载，待迁移 | 遥控、驱动、CAN、EC800 |
| `CageDumper` | STM32F105R / LQFP64 | 裸机 | CMake | 旧 Boot 通过 HTTP 下载，待迁移 | 8 路输入与电机 CAN |

“已使用公共 Boot”表示产品仓库不再保存私有 `boot/` 源码，正式脚本直接消费公共平台冻结的 `ABox_Boot.bin`。“待迁移”表示当前 App 尚未具备 Boot V2 所需的 CA/UFS 下载、稳定槽和 Boot State V3 流程，不能直接替换 Boot 二进制。

## Boot V2 地址合同

| 区域 | 地址范围 | 合同 |
|---|---|---|
| Boot 代码 | `0x08000000..0x08007EFF` | 不得覆盖描述符 |
| Boot 描述符 | `0x08007F00..0x08007FFF` | 256 字节只读描述符，ABI 1 |
| App | `0x08008000..0x0803E7FF` | 最大 `0x36800` 字节 |
| Boot State A | `0x0803E800..0x0803EFFF` | Boot State V3 A 页 |
| Boot State B | `0x0803F000..0x0803F7FF` | Boot State V3 B 页 |
| 产品配置 | `0x0803F800..0x0803FFFF` | 保持各产品已有数据布局 |

工厂或维修 Full 包可以包含 Boot、描述符和 App，但不得写入 Boot State A/B 或产品配置页。

## 当前公共 Boot 发布线

| 产品 | App 交付产物 | Boot 交付产物 | 当前 Boot |
|---|---|---|---|
| `Top_Flying_Wing` | `TopFlyingWing_App.bin` | `ABox_Boot.bin` | `abox-boot-2.3.0` |
| `LockCtrlBoard_cheweishi` | `LockCtrlBoard_App.bin` | `ABox_Boot.bin` | `abox-boot-2.3.0` |
| `Meal_Delivery_Vehicle` | `MealDeliveryVehicle_App.bin` | `ABox_Boot.bin` | `abox-boot-2.3.0` |
| `Sweeper_VCU` | `Sweeper_VCU_App.bin` | `ABox_Boot.bin` | `abox-boot-2.3.0` |

以上四个产品均使用 `otaScheme=boot_v2_ufs`、`otaDownloader=app`、描述符 ABI 1 和特性掩码 `0x0000000F`。平台必须通过真实 `get_info` 获得 `otaReady=true`，不能从公共心跳、构建结果或 MQTT 确认推断设备具备 OTA 能力。

## 旧架构迁移限制

`Airport_vehicle` 和 `CageDumper` 当前仍由 Boot 执行 HTTP 下载，App 只写旧 OTA 请求页。切换公共 Boot V2 前必须：

1. 在 App 中接入 CA/UFS 下载、镜像校验、稳定槽初始化和 Boot State V3；
2. 调整内部 OTA 协议和 `get_info` 能力字段；
3. 明确旧设备是否允许一次性线刷 Full 包；
4. 完成真实设备上的安装、断电恢复、试运行确认和回滚测试；
5. 完成后再删除产品私有 `boot/`。
