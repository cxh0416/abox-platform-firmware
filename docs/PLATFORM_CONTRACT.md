# A-Box Platform Contract

## Boundary

The platform owns scheduler-neutral interfaces and reusable handwritten firmware
components. CubeMX `.ioc` files, generated HAL/CMSIS sources, and product
business protocols remain in each product repository during the migration.

The first extracted component is the byte-identical cJSON implementation under
`components/cjson`; products with a deliberately divergent cJSON copy can set
`ABOX_PLATFORM_USE_CJSON=OFF` while their protocol behavior is being compared.

## Compatibility

- Existing MQTT topics, JSON fields, CAN/RS485 frames, OTA addresses, and artifact
  names are public contracts and must not change as part of platform extraction.
- Product OTA/configuration records are decoded through product adapters. A
  platform update must not erase or reinterpret an existing field layout.
- A platform commit is build metadata; it is not added to an existing public
  device payload unless that protocol is explicitly versioned.

## Scheduler ports

`abox_platform_port.h` is independent of FreeRTOS. Bare-metal products provide
direct callbacks; FreeRTOS products provide callbacks from their task/queue
adapter. Shared code may not include FreeRTOS headers.

## Release gate

Every product release records the product commit, platform commit, compiler
version, App/Boot sizes, Flash layout result, and SHA-256 values. Build success
does not constitute hardware or OTA validation.

Product wrappers call `tools/build_product_release.ps1`, which emits App, Boot,
optional combined images, and `release.manifest.json` under the product's
`dist/` directory.
