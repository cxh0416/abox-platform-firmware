# STM32F105 A-Box 公共 Boot

这是公共平台拥有的完整、可独立构建 Boot V2 目标。硬件合同为 `STM32F105RCT6 + EC800`，固定 Boot/App/Boot State/配置页及 RAM 故障邮箱地址；它不包含任何产品业务或产品 MQTT 协议。

工程内包含 CubeMX `.ioc`、HAL/CMSIS、启动文件、链接脚本、STM32F105/EC800 硬件适配、Boot V2 状态机、构建校验工具和冻结制品。产品仓库不得复制或局部修改这些内容。

构建冻结制品：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_release.ps1
```

命令会生成并验证 `dist/ABox_Boot.bin` 和 `dist/manifest.json`。当前冻结版本为 `abox-boot-2.3.0`，SHA-256 为 `981409ef107d5f5c56a1a80fce107c0292e2c1165668c0279ba6158f24a61c63`。产品仓库日常发布应直接消费这个固定制品，不重新编译 Boot。

该 Boot 不包含 HTTP、HTTPS、CA、产品 Topic 或固件 URL；网络下载由产品 App 负责。构建与静态校验不代表真实设备上的跳转、UFS 安装、断电恢复或回滚已经通过。
