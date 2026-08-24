# STM32F105 A-Box Boot

这是公共平台拥有的完整、可独立构建 Boot V2 目标。硬件契约为 STM32F105RCT6 + EC800，固定 Boot/App/Boot State/配置页及 RAM 故障邮箱地址；它不包含任何产品业务或产品 MQTT 协议。

构建冻结制品：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_release.ps1
```

命令会生成并验证 `dist/ABox_Boot.bin` 和 `dist/manifest.json`。产品仓库日常发布应直接消费这个固定制品，不重新编译 Boot。
