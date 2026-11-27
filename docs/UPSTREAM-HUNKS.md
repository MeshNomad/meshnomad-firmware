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
| `examples/companion_radio/MyMesh.cpp` | `MemStatus.h`, `SerialWifiAuthInterface.h` includes; `meshnomad_wifi_guard_status` declaration (top) | Headers for `get mem`, `get tcp.status`, `get wifi.guard` | Re-add the includes and the declaration under their `#ifdef`s. |
| `examples/companion_radio/MyMesh.cpp` | `MyMesh::clampFutureContacts`, `MyMesh::saveAndReboot` | Contact times ahead of an NTP server time are pulled back (B85); restart that saves contacts first (WiFi guard, #23) | Re-add both functions before `MyMesh::begin()`. |
| `examples/companion_radio/MyMesh.cpp` | `"set tcp.password "`, `"get tcp.password"`, `"set tcp.clear"`, `"set tcp.auth "`, `"get tcp.auth"`, `"get tcp.status"` (`MN_TCP_AUTH`) | CLI of the companion TCP login (B89, B91) | Re-add the block in `handleCommand` next to the WiFi commands. |
| `examples/companion_radio/MyMesh.cpp` | `"set wifi.guard "`, `"get wifi.guard"` (`MN_WIFI_GUARD`) | WiFi guard switch and counters (#23) | Re-add the block after the TCP login block. |
| `examples/companion_radio/MyMesh.cpp` | `"get mem"` | RAM report on the companion | Re-add the `#if defined(ESP32)` block next to `get features`. |
| `examples/companion_radio/MyMesh.h` | `clampFutureContacts`, `saveAndReboot` | Declarations of the two functions above | Re-add in the public section. |
| `examples/companion_radio/NodePrefs.h` | `TcpAuthConfig.h`, `WifiGuardProbe.h` includes; `tcp_pwd`, `tcp_auth`, `wifi_guard`; `def("tcp_pwd"`, `def("tcp_auth"`, `def("guard"` | Stored settings of the TCP login and the WiFi guard | Re-add the includes, the members and their `def()` lines under `MN_TCP_AUTH` / `MN_WIFI_GUARD`. |
| `examples/companion_radio/main.cpp` | `SerialWifiAuthInterface wifi_interface;` (`MN_TCP_AUTH`) | TCP link with login and encryption replaces `SerialWifiInterface` | Keep our `#if defined(MN_TCP_AUTH)` branch in front of upstream's interface choice. |
| `examples/companion_radio/main.cpp` | `wifi_interface.setAuth(`, `wifi_interface.setIdentity(` in `setup()` | Password and switch read at every login; the node key signs each handshake | Re-add before `wifi_interface.begin(TCP_PORT)`. |
| `examples/simple_repeater/MyMesh.cpp`, `examples/simple_room_server/MyMesh.cpp` | `RegionLoadGuard.h` include; `mesh::regionLoadLine(` at the top of `handleCommand`; `mesh::regionLoadIsStart(command)` after the prefix strip; `region_load_at = millis();` in `startRegionsLoad` | `region load` refused from a remote sender; an open load takes local lines only and ends on an app-prefixed line or 5 min of silence (B87) | Re-add the include and the three `#ifdef MESHNOMAD_BUILD` blocks; the `#else` keeps upstream's `if (region_load_active) {`. |
| `examples/simple_repeater/MyMesh.h`, `examples/simple_room_server/MyMesh.h` | `uint32_t region_load_at` | Time of the load's last line (B87) | Re-add after `region_load_active`. |
| `src/helpers/ArduinoSerialInterface.cpp/.h` | `SerialFrameGuard.h` include, `mesh::serialFrameStale(`, `mesh::serialFrameLengthOk(`, `_last_byte_at` | A cut frame is dropped after a 1 s gap, an impossible length at once (B90) | Re-add the include, the member and the three `#ifdef MESHNOMAD_BUILD` blocks in `checkRecvFrame`. |
| `src/helpers/BaseChatMesh.cpp` | `RtcBootstrap.h` include, `rtcBootstrapWanted(` in `bootstrapRTCfromContacts` | A clock that is already set is not replaced by the newest contact time at boot | Re-add the include and the early return. |
| `src/helpers/BaseChatMesh.cpp/.h` | `BaseChatMesh::clampContactsLastmod` | Pulls contact times that are ahead of the clock back to it (B85) | Re-add the function and its declaration (protected). |
| `src/helpers/CommonCLI.cpp` | `MemStatus.h`, `GpsStatus.h` includes | Needed by `get mem` and the `gps` reply | Re-add both includes. |
| `src/helpers/CommonCLI.cpp` | `mesh::formatGpsStatus(` in the `gps` command | `gps` on a node without a GPS module no longer reads a NULL setting (B88) | Replace upstream's reply code of the plain `gps` command with the call. |
| `src/helpers/CommonCLI.cpp` | `strcmp(config, "mem")` in `handleGetCmd` | `get mem` on repeater and room server | Re-add the `#if defined(ESP32)` branch in the `get` chain. |
| `src/helpers/sensors/EnvironmentSensorManager.cpp` | `SENSOR_TABLE` rows `0x41` and `0x43` for `INA3221` | INA3221 on any A0 strapping | Re-add the two `#if TELEM_INA3221_ADDRESS != ...` rows, `0x41` before INA260. |
| `boards/t_beams3_supreme.json` | `"-DBOARD_HAS_PSRAM"` | 8 MB PSRAM in use on the T-Beam S3 Supreme (companion with WiFi ran out of internal RAM) | Re-add the flag in `extra_flags`. |

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
| `platformio.ini` | `test_ignore` in `[env:native]` (`test_tcp_auth_gate`, `test_tcp_secure_channel`); `[env:native_tcp_crypto]` | TCP protocol tests run with the real Crypto and ed25519 libraries, not the mocks | Re-add the `test_ignore` lines and the whole env. |
| `.github/workflows/firmware-builder.yml` | `permissions:`, `persist-credentials: false`, `actions/attest`, `Determine release notes range`, `git-cliff==2.14.2`, `Assemble release notes`, `generate_release_notes: false` | Least-privilege token, build provenance, release notes from git-cliff since the previous published release; actions pinned by SHA | Keep ours; third-party actions must stay on the repository's allow-list (run tools with `pipx run`). |
| `.github/workflows/pr-build-check.yml`, `.github/workflows/run-unit-tests.yml` | `permissions: contents: read`, `persist-credentials: false`, `ENVIRONMENT: ${{ matrix.environment }}` | Least-privilege token; no template expansion inside `run:` | Keep ours. |
| `.github/actions/setup-build-environment/action.yml` | `Unexpected firmware version`, pinned `actions/cache` and `actions/setup-python` | The version string is checked against a strict pattern before it reaches `GITHUB_ENV`; actions pinned by SHA | Keep ours. |
| `.gitignore` | `.cliffignore` and the other MeshNomad entries at the end | Generated and local files stay out of git | Keep our lines. |
| `README.md` | whole file | MeshNomad README replaces MeshCore's | Keep ours; do not take upstream README changes. |

## Deleted upstream files

A MeshCore merge brings these back; delete them again (operator 2026-10-26, B22/B23).

| File | Purpose of the deletion | How to re-apply on a MeshCore update |
|---|---|---|
| `.github/workflows/github-pages.yml`, `mkdocs.yml`, `docs/docs.md` | No MeshCore documentation site (GitHub Pages / MkDocs) in this repo | Delete again. |
| `CONTRIBUTING.md` | MeshCore's contribution rules do not apply to MeshNomad | Delete again. |
| `SECURITY.md` | MeshCore's version table and reporting channel do not apply; the file in this repo is MeshNomad's own (since 2026-11-01) | Keep ours; do not take upstream's. |
| `docs/index.md`, `docs/_stylesheets/`, `docs/_assets/`, `docs/cli_commands.md`, `docs/companion_protocol.md`, `docs/faq.md`, `docs/kiss_modem_protocol.md`, `docs/nrf52_power_management.md`, `docs/number_allocations.md`, `docs/packet_format.md`, `docs/payloads.md`, `docs/qr_codes.md`, `docs/radio_presets.md`, `docs/stats_binary_frames.md`, `docs/terminal_chat_cli.md` | MeshCore documentation (MkDocs leftovers and protocol reference) is not kept in this repo; README links MeshCore's `docs/` on GitHub. `docs/UPSTREAM-HUNKS.md`, `docs/assets/` and `docs/superpowers/` are fork-only and stay. | Delete again (operator 2026-10-26). |
