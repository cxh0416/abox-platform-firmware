# A-Box 公共固件平台

当前平台版本为 `0.2.3`。平台同时提供可复用固件组件，以及面向统一 STM32F105/EC800 硬件基线的完整 Boot V2 工程。

## 目录说明

- `include/`、`src/`：产品配置校验、硬件回调端口和平台版本接口。
- `components/boot_v2/`：Boot State V3、描述符、故障邮箱、App 下载端和 Boot 安装端。
- `components/ota/`：旧架构使用的 EC800 AT 与直接写 Flash 组件。
- `components/ec_power/`：EC800 上电、关机和重启时序。
- `components/cjson/`：平台统一使用的 cJSON。
- `cmake/`：产品 App、旧 Boot 和 Boot V2 的 CMake 接入函数。
- `targets/stm32f105_abox_boot/`：完整、可独立构建的公共 Boot 工程。
- `tests/`：公共组件主机测试。
- `tools/`：Flash 布局、产品矩阵、基线和产品发布校验工具。
- `docs/`：平台合同、产品迁移矩阵和基线记录。

`build-host/` 和各目标下的 `build/` 是本地构建缓存，不属于平台源码。

## 公共 Boot

公共 Boot 目标适用于 `STM32F105RCT6 + EC800` 的 `abox_stm32f105_ec800_v1` 硬件合同。当前冻结版本为 `abox-boot-2.3.0`：

- Boot：`0x08000000..0x08007FFF`；
- 描述符：`0x08007F00..0x08007FFF`；
- App：`0x08008000..0x0803E7FF`；
- Boot State A/B：`0x0803E800`、`0x0803F000`；
- 产品配置页：`0x0803F800`；
- 冻结产物：`targets/stm32f105_abox_boot/dist/ABox_Boot.bin`；
- SHA-256：`981409ef107d5f5c56a1a80fce107c0292e2c1165668c0279ba6158f24a61c63`。

Boot 不连接业务平台，也不下载网络固件。产品 App 负责 HTTPS、CA、EC800 UFS 和 OTA Topic；Boot 只负责安装、试运行确认和回滚。

## 产品接入

产品 App 通过 Git 子模块固定平台提交，并在自身 CMake 中调用：

```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/../platform" abox_platform_build)
abox_platform_attach(Product_App)
abox_platform_attach_boot_v2_app(Product_App)
```

产品仓库继续拥有业务协议、CAN/RS485、执行器逻辑、MQTT Topic、产品配置结构、App 硬件适配和发布包装。不得在产品仓库复制或局部修改公共 Boot。

当前 `Top_Flying_Wing`、`LockCtrlBoard_cheweishi`、`Meal_Delivery_Vehicle`、`Sweeper_VCU` 已接入公共 Boot；`Airport_vehicle`、`CageDumper` 仍为旧 Boot 网络下载架构，切换前必须先改造 App，并确认旧设备迁移方案。

## 构建与验证

```powershell
cmake -S . -B build-host -G Ninja -DBUILD_TESTING=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure

powershell -ExecutionPolicy Bypass -File .\targets\stm32f105_abox_boot\tools\build_release.ps1
python .\targets\stm32f105_abox_boot\tools\verify_boot_artifact.py `
  .\targets\stm32f105_abox_boot\dist\ABox_Boot.bin
```

构建、主机测试和 MQTT 确认都不能替代真实设备上的 Boot 跳转、UFS 安装、断电恢复、试运行确认和回滚验收。
