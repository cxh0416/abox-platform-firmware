# A-Box Product Matrix

| Product | MCU/package | Runtime | Build baseline | OTA integration | Product-specific boundary |
|---|---|---|---|---|---|
| Airport_vehicle | STM32F105R / LQFP64 | Bare-metal | Keil, CMake migration planned | Existing product integration | RC/motor/CAN/EC800 |
| CageDumper | STM32F105R / LQFP64 | Bare-metal | CMake | Existing product integration | 8-input and motor CAN |
| LockCtrlBoard_cheweishi | STM32F105R / LQFP64 | Bare-metal | CMake + Keil | Boot V2, App downloader | Lock RS485 and temperature bus |
| Meal_Delivery_Vehicle | STM32F105R / LQFP64 | Bare-metal | CMake | Boot V2, App downloader | Dual-lock RS485/MQTT |
| Sweeper_VCU | STM32F105R / LQFP64 | FreeRTOS | CMake | Boot V2, App downloader | VCU tasks/IWDG/dual CAN |

## Boot V2 Flash contract

The three migrated products use the same STM32F105 Flash layout:

| Region | Address range | Contract |
|---|---|---|
| Boot code | `0x08000000..0x08007EFF` | Must not overlap the descriptor |
| Boot descriptor | `0x08007F00..0x08007FFF` | 256-byte read-only descriptor, ABI 1 |
| App | `0x08008000..0x0803E7FF` | Maximum size `0x36800` bytes |
| Boot State A | `0x0803E800..0x0803EFFF` | Boot State V3 page A |
| Boot State B | `0x0803F000..0x0803F7FF` | Boot State V3 page B |
| Product configuration | `0x0803F800..0x0803FFFF` | Existing product-owned data layout |

Combined factory images may include Boot, descriptor, and App, but must not
write Boot State A/B or the product configuration page. The Boot image including
its descriptor remains within the 32 KiB Boot allocation.

## Boot V2 release line

| Product | App revision artifact | Boot minimum/current line |
|---|---|---|
| LockCtrlBoard_cheweishi | `lockctrlboard_boot_v2_app.bin` | `2.2.0` |
| Meal_Delivery_Vehicle | `meal_delivery_vehicle_app.bin` | `2.2.0` |
| Sweeper_VCU | `sweeper_vcu_app.bin` | `2.2.0` |

All three advertise `otaScheme=boot_v2_ufs`, `otaDownloader=app`, descriptor
ABI 1, required feature mask `0x0000000F`, and `otaReady` obtained from a real
`get_info` response. Public heartbeat and product business payload contracts do
not change as part of this migration.
