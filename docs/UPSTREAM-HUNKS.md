# Upstream hunks

MeshNomad changes inside files that come from MeshCore (files that exist on `main`). Fork-only
files (`src/helpers/net/`, `src/helpers/MeshNomad*.h`, `*/MeshNomadNet.cpp`, `tool/meshnomad/`,
`variants/meshnomad/`, `build-dev-firmware.yml`) are not listed. After every MeshCore merge,
re-apply or re-check each row below. This is the input for BACKLOG B34. Anchors are search
strings, not line numbers. Inventory from `git diff main --diff-filter=M`.

## Firmware code

| File | Anchor | Purpose | How to re-apply on a MeshCore update |
|---|---|---|---|
| `examples/companion_radio/MyMesh.cpp` | `#include <helpers/net/NetHooks.h>` (top) | Net hook header, `MeshNomadFeatures.h` include | Re-add both includes. |
| `examples/companion_radio/MyMesh.cpp` | `net_observer_rx_raw` in `logRxRaw` | Observer gets every received packet | Re-add the `#ifdef WITH_NET_SERVICES` call at the end of `logRxRaw`. |
| `examples/companion_radio/MyMesh.cpp` | `net_handle_cli(command, reply, 157)` (B29) | Net CLI hook, after the app's `xx\|` prefix strip | Re-add in `handleCommand` right after the prefix strip, before upstream commands. |
| `examples/companion_radio/MyMesh.cpp` | `"get features"`, `"get build"` | Features/build info for the app | Re-add both `if` blocks next to the other `get` commands. |
| `examples/companion_radio/MyMesh.cpp` | `do not echo the WiFi password (PWD)` | `get wifi.pwd` replies `(set)` / `(not set)` | Re-apply in the `get wifi.pwd` branch (`ENABLE_WIFI_INTERFACE`). |
| `examples/companion_radio/NodePrefs.h` | `PATH_HASH_MODE_DEFAULT` | 2-byte path hash default | Re-add the `#ifndef` block; use the macro as the `path_hash_mode` initializer. |
| `examples/companion_radio/main.cpp` | `meshnomad_net_setup` / `meshnomad_net_loop` | Net layer start and loop | Re-add the declarations and the calls at the end of `setup()` and in `loop()`. |
| `examples/companion_radio/main.cpp` | `!defined(MESHNOMAD_BUILD)` after `#if defined(DISPLAY_CLASS)` | Fixed BT PIN, no random PIN | Keep the added condition on the random-PIN block. |
| `examples/companion_radio/ui-new/UITask.cpp` | `MeshNomadBranding.h`, `meshnomadBootInfo`, `BOOT_LOGO`, `BOOT_WEBSITE`, `_build_info` | Boot screen branding, version and build lines | Re-add include, `_version_info[24]`, `_build_info`, `#ifdef MESHNOMAD_BUILD` blocks, replace logo/website constants. |
| `examples/simple_repeater/MyMesh.cpp` | `NetHooks.h` include (top) | Net hook header | Re-add include. |
| `examples/simple_repeater/MyMesh.cpp` | `MyMesh::getRepeaterStats`, `getNeighbourCount`; `getRepeaterStats(stats);` in `handleRequest` | Stats and neighbour accessors for MQTT; stats code moved out of `handleRequest` | Re-extract the stats block into `getRepeaterStats`, re-add `getNeighbourCount`; re-check against upstream stats fields. |
| `examples/simple_repeater/MyMesh.cpp` | `net_observer_rx_raw` in `logRxRaw` | Observer hook | Re-add the `#ifdef` call. |
| `examples/simple_repeater/MyMesh.cpp` | `net_handle_cli(command, reply, 157)` (B29) | Net CLI hook after prefix strip | Re-add in `handleCommand` after the prefix strip. |
| `examples/simple_repeater/MyMesh.h` | `Telemetry accessors for the network services` | Declarations of the two accessors | Re-add after the public section's last method. |
| `examples/simple_repeater/UITask.cpp` | `MeshNomadBranding.h`, `meshnomadBootInfo`, `BOOT_Y_*`, `BOOT_LOGO`, `BOOT_WEBSITE` | Boot screen branding and layout constants | Re-add include and `#ifdef MESHNOMAD_BUILD` blocks; replace literal Y offsets, logo, website. |
| `examples/simple_repeater/UITask.h` | `char _build_info[24]` | Build line buffer | Re-add under `#ifdef MESHNOMAD_BUILD`. |
| `examples/simple_repeater/main.cpp` | `meshnomad_net_setup` / `meshnomad_net_loop` | Net layer start and loop | Re-add declarations and calls. |
| `examples/simple_room_server/MyMesh.cpp` | `NetHooks.h` include (top) | Net hook header | Re-add include. |
| `examples/simple_room_server/MyMesh.cpp` | `net_observer_rx_raw` in `logRxRaw` | Observer hook | Re-add the `#ifdef` call. |
| `examples/simple_room_server/MyMesh.cpp` | `net_handle_cli(command, reply, 157)` (B29) | Net CLI hook after prefix strip | Re-add in `handleCommand` after the prefix strip. |
| `examples/simple_room_server/UITask.cpp` | `MeshNomadBranding.h`, `meshnomadBootInfo`, `BOOT_Y_*`, `BOOT_LOGO`, `BOOT_WEBSITE`, `_build_info` | Boot screen branding and layout | As repeater UITask. |
| `examples/simple_room_server/UITask.cpp` | `_started_at` | Boot screen lasts `BOOT_SCREEN_MILLIS` from `begin()`, not from power-up | Set `_started_at = millis()` in `begin()`; compare against `_started_at + BOOT_SCREEN_MILLIS`. |
| `examples/simple_room_server/UITask.h` | `_started_at`, `_build_info` | Members for the two rows above | Re-add. |
| `examples/simple_room_server/main.cpp` | `meshnomad_net_setup` / `meshnomad_net_loop` | Net layer start and loop | Re-add declarations and calls. |
| `src/helpers/CommonCLI.cpp` | `MeshNomadFeatures.h`, `LPPDataHelpers.h` includes | Needed by `sensor read` and `get features` | Re-add both includes. |
| `src/helpers/CommonCLI.cpp` | `"sensor read"` in `handleCommand` | Live sensor values over CLI | Re-add the `else if` block with the other `sensor` commands. |
| `src/helpers/CommonCLI.cpp` | `"sensor scan"` in `handleCommand` | I2C bus scan over CLI | Re-add the `else if` block (needs `scanBus` rows below). |
| `src/helpers/CommonCLI.cpp` | `strcmp(config, "features")`, `strcmp(config, "build")` in `handleGetCmd` | `get features`, `get build` | Re-add as the first branches of the `get` chain (before `allow.read.only`). |
| `src/helpers/CommonCLI.h` | `PATH_HASH_MODE_DEFAULT` (define + initializer) | 2-byte path hash default | Re-add `#ifndef` block; use macro in `path_hash_mode`. |
| `src/helpers/ArduinoSerialInterface.cpp/.h` | `APP_FRAME_TIMEOUT_MILLIS`, `_last_frame_at` | USB counts as connected only while the app sends frames (BT PIN stays visible) | Re-add the `#ifdef MESHNOMAD_BUILD` `isConnected()`, the `_last_frame_at` update in `checkRecvFrame`, the member. |
| `src/helpers/SensorManager.h` | `virtual int scanBus` | Base hook for `sensor scan` | Re-add the virtual with default `-1`. |
| `src/helpers/sensors/EnvironmentSensorManager.h` | `int scanBus(...) override;` | `sensor scan` implementation | Re-add. |
| `src/helpers/sensors/EnvironmentSensorManager.cpp` | `INA226_ALT`, `MN_INA226_ALT`, `init_ina226_alt`, `query_ina226_alt`, `SENSOR_TABLE` rows for `0x40` | INA226 / INA3221 also on 0x40 (ID-checked) | Re-add the static instance, the two functions and the two table rows; keep order before INA219. |
| `src/helpers/sensors/EnvironmentSensorManager.cpp` | `EnvironmentSensorManager::scanBus` (end of file) | I2C scan | Re-add the function. |

## Build and CI

| File | Anchor | Purpose | How to re-apply on a MeshCore update |
|---|---|---|---|
| `platformio.ini` | `PATH_HASH_MODE_DEFAULT=1` in `build_flags` | 2-byte path hash default | Re-add the flag. |
| `build.sh` | `-DMESHNOMAD_BUILD_ENV=` in `PLATFORMIO_BUILD_FLAGS` (`build_firmware`) | Build env name compiled in | Re-add to the flags line. |
| `build.sh` | `get_pio_envs_ending_with_string "_mn_cmp"` / `_mn_rpt` / `_mn_rs` | Only MeshNomad envs are built in CI | Keep the three suffixes in the `get-*-firmwares-to-build` branches. |
| `build.sh` | usage text `heltec_v4_mn_rpt` | Examples use MeshNomad env names | Cosmetic; re-apply or drop. |
| `merge-bin.py` | `__get_board_flash_mode(__env__)` | Flash mode taken from the board at build time | Re-apply the one-line replacement of `board_config.get("build.flash_mode", "dio")`. |
| `.github/actions/setup-build-environment/action.yml` | `VERSION_STRING="v${GIT_TAG_NAME#*-v}"`, `-meshnomad.dev` | Version string from `rpt-v…`/`cmp-v…`/`rs-v…` tags; dev builds | Keep our tag parsing and dev version block. |
| `.github/workflows/build-companion-firmwares.yml` | `'cmp-v*'`, `release_title_prefix: 'CMP'` | Tag pattern and release title | Keep. |
| `.github/workflows/build-repeater-firmwares.yml` | tag pattern, release title prefix | Same for `rpt-v*` | Keep. |
| `.github/workflows/build-room-server-firmwares.yml` | tag pattern, release title prefix | Same for `rs-v*` | Keep. |
| `.github/workflows/firmware-builder.yml` | `concurrency:`, `partition_changes.py`, `body_path: release-notes.md`, release `name:` | Duplicate-tag-push guard, partition diff in release notes, release title | Re-add the three blocks and the title. |
| `.github/workflows/pr-build-check.yml` | `branches: [dev-meshnomad, main-meshnomad]`, `paths:`, env matrix | PR checks on our branches and our build envs | Keep ours; take only structural upstream changes. |
| `.github/workflows/run-unit-tests.yml` | `branches: [dev-meshnomad, main-meshnomad]`, `paths-ignore`, no `push:` | Unit tests only on PRs to our branches, skip docs-only | Keep ours. |
| `README.md` | whole file | MeshNomad README replaces MeshCore's | Keep ours; do not take upstream README changes. |

## Deleted upstream files

A MeshCore merge brings these back; delete them again (operator 2026-10-26, B22/B23).

| File | Purpose of the deletion | How to re-apply on a MeshCore update |
|---|---|---|
| `.github/workflows/github-pages.yml`, `mkdocs.yml`, `docs/docs.md` | No MeshCore documentation site (GitHub Pages / MkDocs) in this repo | Delete again. |
| `SECURITY.md`, `CONTRIBUTING.md` | MeshCore's version table, reporting channel and contribution rules do not apply to MeshNomad | Delete again. |
