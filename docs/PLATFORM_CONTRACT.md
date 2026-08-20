# A-Box Platform Contract

This document describes the current `ABox_Platform v0.2.3` integration contract.

## Boundary

The platform owns scheduler-neutral interfaces and reusable handwritten firmware
components. CubeMX `.ioc` files, generated HAL/CMSIS sources, product business
protocols, linker scripts, EC800 ownership policy, and hardware adaptation remain
in each product repository.

The reusable component groups are:

- `abox::core`, `abox::cjson`, `abox::ec_power`, `abox::ota`, and `abox::boot` for
  existing integrations;
- `abox::boot_v2_common` for CRC32, vector validation, fault mailbox, Boot State
  V3, descriptor validation, and state names;
- `abox::boot_v2_app` for CA/UFS setup, HTTPS Range download, image verification,
  stable-slot provisioning, and `INSTALL_PENDING` submission;
- `abox::boot_v2_boot` for UFS installation, interrupted-install recovery,
  `TRIAL`, three-failure rollback, and rollback-report retention.

Existing `abox::ota`, `abox::boot`, `abox_platform_attach`, and
`abox_platform_attach_boot` interfaces remain available for products that have
not migrated. Boot V2 products opt in with `abox_platform_attach_boot_v2_app`
and `abox_platform_attach_boot_v2`; migration must not change an unrelated
product implicitly.

## Compatibility

- Existing MQTT topics, JSON fields, CAN/RS485 frames, OTA addresses, and artifact
  names are public contracts and must not change as part of platform extraction.
- Product OTA/configuration records are decoded through product adapters. A
  platform update must not erase or reinterpret an existing field layout.
- A platform commit is build metadata; it is not added to an existing public
  device payload unless that protocol is explicitly versioned.
- `ABoxBootV2Record` is a persistent ABI: Version 3, 112 bytes, with the existing
  numeric state values. Changes require an explicit disk-format migration.
- The 256-byte Boot descriptor is a read-only ABI at `0x08007F00`. It uses magic
  `0x32564241`, ABI 1, and CRC32 over all bytes before the final CRC field.
- Required Boot V2 features are State V3, UFS A/B, App staging, and rollback
  reporting (`0x0000000F`). A missing, invalid, or incomplete descriptor makes
  the App OTA-ineligible.

## Scheduler ports

`abox_platform_port.h` is independent of FreeRTOS. Bare-metal products provide
direct callbacks; FreeRTOS products provide callbacks from their task/queue
adapter. Shared code may not include FreeRTOS headers.

Boot V2 uses callback ports rather than product HAL symbols. The App port owns
AT submission, raw receive, MQTT pause/resume, transfer buffering, time, and
logging. The Boot port owns UFS installation, Flash/vector checks, reset reason,
App jump, delay, and logging. Product adapters decide how those callbacks map to
HAL, EC800, and scheduler services.

## Boot V2 behavior

- The App downloads an immutable HTTPS revision in 1024-byte Range chunks and
  requires HTTP `206` with the exact requested length.
- The App validates URL, size, CRC32, and vector table before atomically writing
  `INSTALL_PENDING`. Network download code is not linked into Boot.
- A newly flashed App provisions the running immutable revision into the stable
  UFS slot before reporting `otaReady=true`. Provisioning failure must not stop
  local business, but it blocks platform OTA. The shared App component verifies
  the existing CA file before reuse, repairs a missing or mismatched file, retries
  transient probe/CA failures after 5, 15, and 60 seconds, and retries terminal
  failures every five minutes. Probe failures report `1001`; CA file operations
  report `1002`.
- A failed automatic stable-slot download clears the one-shot attempt guard before
  transport reprovisioning, so the five-minute retry can attempt the immutable
  running revision again without requiring another MCU reset.
- Boot installs only from UFS, records `INSTALLING` before destructive work,
  enters `TRIAL` after verification, and restores the stable slot after three
  qualifying trial failures.
- `ROLLBACK` remains reportable until the platform receives a `get_info`
  response and the App acknowledges that report. Version alone never proves an
  OTA task complete.

## Release gate

Every product release records the product commit, platform commit, compiler
version, App/Boot sizes, Flash layout result, and SHA-256 values. Build success
does not constitute hardware or OTA validation.

Each product owns its build/package wrapper and artifact names. The shared
`tools/validate_flash_layout.ps1` enforces the common address contract; product
wrappers emit App, Boot, combined images where supported, and a manifest with
addresses, sizes, CRC32, SHA-256, product commit, platform commit, and compiler
identity under that product's `dist/` directory.
