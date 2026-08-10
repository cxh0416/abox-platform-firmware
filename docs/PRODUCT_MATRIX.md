# A-Box Product Matrix

| Product | MCU/package | Runtime | Build baseline | Product-specific boundary |
|---|---|---|---|---|
| Airport_vehicle | STM32F105R / LQFP64 | Bare-metal | Keil, CMake migration planned | RC/motor/CAN/EC800 |
| CageDumper | STM32F105R / LQFP64 | Bare-metal | CMake | 8-input and motor CAN |
| LockCtrlBoard_cheweishi | STM32F105R / LQFP64 | Bare-metal | CMake + Keil | Lock RS485 and temperature bus |
| Meal_Delivery_Vehicle | STM32F105R / LQFP64 | Bare-metal | CMake | Lock RS485/MQTT |
| Sweeper_VCU | STM32F105R / LQFP64 | FreeRTOS | CMake | VCU tasks/IWDG/dual CAN |

Common intended Flash contract: Boot `0x08000000..0x08007FFF`, App start
`0x08008000`, OTA/configuration page `0x0803F800..0x0803FFFF`.
