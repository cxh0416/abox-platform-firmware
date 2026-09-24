# Platform 模块结构整理验证记录（2026-09-24）

本记录对应 Platform `b212b33417879aa0667ff4bdba5ec586568492c0`（`v0.3.0`）到本次目录整理。改动限于源码/头文件移动、CMake target 定义归属和三个产品的路径接入。所有平台 C/H 文件均为 Git 100% rename，函数及实现正文未改。

## 构建配置与制品对比

三个产品均使用 ARM GNU Toolchain 14.2.Rel1、Ninja、`Release`、原有 App target 与原有版本/功能配置。整理前重新构建并取得下表基线；整理后在同一构建目录和配置重新构建。SHA256 为 App BIN 全文件哈希，也与各自现有 `dist` App 相同。

| 产品（整理前源码提交） | 配置要点 | 整理前后 App SHA256 | App 字节 | Flash/RAM 字节 |
| --- | --- | --- | ---: | ---: |
| 巡防 `2c5c6e8` | `patrol-chassis-0.2.35`，运动/台架选项均 OFF | `689776FCFF9F1A5DB064EC2DF053B3DD39DF72CBBA6F951D00114F9842B35FF2` | 126312 | 126312 / 61432 |
| 送餐 `2b35096` | `Release`，原 App CMake 默认值 | `23A6A975083AE66F4F75D02786DEA64AAC1D3DEF50BB2736DCE8841A87127103` | 107300 | 107300 / 54692 |
| 清扫 `3f3de17` | `sweeper-vcu-1.3.5` | `9D50386CF860EBDEBBC6E02A73BFA3E2D9D35EFF273358FA68A3AA15E6E4129E` | 104388 | 104388 / 61432 |

全文件 BIN 一致证明所覆盖的 App Flash 镜像内容和地址布局一致；RAM 占用由链接器 `--print-memory-usage` 输出核对一致。各产品沿用的冻结 Boot 文件均未移动或重建，SHA256 均为 `981409EF107D5F5C56A1A80FCE107C0292E2C1165668C0279BA6158F24A61C63`。正式 `dist/` 未替换，版本号未递增，也未执行 OTA 或现场验收。

为单独核对链接布局，在独立 `app/build/structure-baseline/` 中临时使用原 Platform 提交和相同配置重建三个基线 App，随后恢复新 Platform 提交。整理前后的 ELF 节表（`objdump -h`）和按地址列出的定义符号（`nm -n --defined-only`）均相同：巡防 20 节/1321 符号、送餐 20 节/1155 符号、清扫 20 节/1245 符号。三份基线 BIN 与整理后 BIN 的 SHA256 也分别相同。

## 测试

- Platform 独立 MinGW/Ninja 构建成功；CTest 21/21 通过，含 CMake 组件选择与依赖契约测试。
- 公共 Boot 工程使用原 ARM 工具链完成 Release CMake configure/generate；按冻结约束未执行 Boot 编译。
- 巡防：Python 主机测试 38 项通过，Platform CTest 21/21，通过 MQTT/Profile、云协议、底盘安全和台架回归。
- 送餐：MQTT Profile、核心适配器、TLS Runtime、Boot V2 契约、公共协议检查通过。
- 清扫：Python 主机测试 27 项通过，Platform CTest 21/21，通过 CAN 控制、MQTT Profile 和云协议回归。
- 三个 App 的 ARM Release 构建成功；`git diff --check` 通过，移动文件保持 LF。
