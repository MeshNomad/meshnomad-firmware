# MeshNomad Firmware

Firmware for LoRa mesh radios used by the MeshNomad app — a fork of
[MeshCore](https://github.com/meshcore-dev/MeshCore) that stays as close to the
original as possible.

> **Not affiliated with the MeshCore project.** This is an independent fork. For
> MeshCore itself, its documentation, community and official releases, go to
> [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).

## What you get

MeshNomad firmware is MeshCore plus:

- **One firmware per board and role, for ESP32 boards.** No choosing between Bluetooth, USB
  and WiFi builds: everything is inside, and the extra features are switched on at runtime
  from the MeshNomad app or the console.
- **Network features:** WiFi, a password-protected network console (repeater, room
  server), MQTT telemetry with Home Assistant discovery, NTP clock sync and an MQTT packet
  observer. All off until you switch them on.
- **Sensor and build commands:** `sensor read`, `sensor scan`, `get features`, which
  lets the MeshNomad app show only the commands your firmware has, and `get build`, which
  tells it which firmware file fits the device.
- **2-byte path hash by default** on a fresh install.

The radio, the protocol and the routing are MeshCore's, so a MeshNomad node works with any
MeshCore node in the same mesh. Details: [Differences from MeshCore](#differences-from-meshcore)
and [Commands](#commands).

## Origin

- Forked from `meshcore-dev/MeshCore`.
- Every MeshCore release is merged into this fork; nothing from this fork is sent
  back upstream.
- Licence: MIT, unchanged — see [`license.txt`](license.txt). All MeshCore code
  keeps its original copyright.

## Fork rule

We practically do not modify code that touches communication — the protocol,
routing and the radio layer stay exactly as in MeshCore, so these nodes work in
any MeshCore mesh. Around it we improve what exists and add our own features.
Some of them are on by default and some MeshCore defaults are changed — on
purpose. Every difference from MeshCore, and whether it is on by default, is
listed below.

## Differences from MeshCore

| Area | What is different | Since |
|---|---|---|
| Releases | Own release builds from `rpt-v…`, `cmp-v…`, `rs-v…` tags (repeater, companion, room server). The firmware reports its version as `v<MeshCore version>-meshnomad.<release>-<commit>`, e.g. `v1.18.0-meshnomad.2-9f3c2a1`; the number after `meshnomad.` counts our releases on that MeshCore version and starts again at 1 on the next one. Development builds report `v1.18.0-meshnomad.dev.<build>-<commit>` — see [Versions](#versions). | 1.18.0 |
| Path hash | `path.hash.mode` defaults to `1` (2-byte path hash) on a fresh install; change it with `set path.hash.mode`. Existing settings are kept. | 1.18.0 |
| CLI | New commands: `sensor read` (live sensor values), `sensor scan` (I2C addresses on the sensor bus), `get features` (what this build contains, used by the MeshNomad app), `get build` (which firmware build this is, e.g. `heltec_v4_mn_rpt`). | 1.18.0 |
| Network (ESP32) | MeshCore has WiFi only in its separate companion WiFi builds (the app connects to the companion over WiFi); the companion here keeps exactly that. This fork's own code (`src/helpers/net/`) adds WiFi for the repeater and room server with a password-protected TCP console on port 5000, MQTT telemetry with Home Assistant discovery, SNTP clock sync and an MQTT packet observer. All of it is off by default and switched on with `set wifi.enabled 1`, `set tcp.enabled 1`, `set mqtt.enabled 1`, `set ntp.enabled 1`, `set obs.enabled 1`; settings via CLI (`wifi.*`, `tcp.enabled`, `mqtt.*`, `ntp.*`, `obs.*`, `net status|scan|apply`). | 1.18.0 |
| Firmware per role | One firmware per ESP32 board and role: repeater `<board>_mn_rpt`, companion `<board>_mn_cmp` (Bluetooth, USB and WiFi at the same time), room server `<board>_mn_rs`. Every role contains the network layer (WiFi, MQTT, NTP, observer; the repeater and room server add a TCP console), all off by default. Boards without WiFi, and boards that cannot hold a role's full feature set, get no MeshNomad firmware — see [Supported boards](#supported-boards). Release builds have debug logging switched off. | 1.18.0 |
| Bluetooth PIN | The companion pairs with PIN `123456` on every board. MeshCore picks a random PIN at every start on boards with a display. A PIN set with `set pin` or in the app still takes precedence. | 1.18.0 |
| Sensors | INA3221 and INA226 current monitors are also found on I2C address 0x40 (identified by their chip ID, so an INA219 on 0x40 still works). | 1.18.0 |

## Commands

Commands added by this fork, or whose default it changes. Where to type them:

- **Repeater, room server:** the USB serial console, the network console (`tcp.enabled 1`),
  the MeshNomad app's repeater / room server console, or remote admin over LoRa.
- **Companion:** its USB port speaks the app protocol, not text, so there is no serial
  console. Commands reach it as remote admin over LoRa from a contact you allowed remote
  CLI, or from an app through the companion protocol's CLI command; the MeshNomad app has
  no companion console yet.

`get <key>` shows the current value, `set <key> <value>` changes and saves it.
Every entry lists the values, the default and an example.

### Path hash

Roles: repeater, room server, companion.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get path.hash.mode` | Shows how many bytes of each node's hash this node writes into the path of the packets it sends. | — | — |
| `set path.hash.mode <n>` | Changes it and saves it: `0` = 1 byte, `1` = 2 bytes, `2` = 3 bytes. Longer hashes tell nodes apart more reliably; shorter ones leave room for longer paths. A node that already has a saved setting keeps it. | `0`, `1`, `2` | `1` (MeshCore: `0`) |

Example:

```
get path.hash.mode
set path.hash.mode 2
```

### Sensors and build info

Roles: `sensor read` and `sensor scan` — repeater, room server; `get features`, `get build` — all roles.

| Command | What it does | Values | Default |
|---|---|---|---|
| `sensor read` | Reads every sensor now and prints one value per line, channel first (`ch2 temp=21.3C`). | — | — |
| `sensor scan` | Lists the I2C addresses that answer on the sensor bus — a quick wiring check. Prints `no I2C devices` when nothing answers. | — | — |
| `get features` | Lists what this firmware build contains, e.g. `wifi mqtt ntp observer gps`; `none` when nothing extra. The MeshNomad app uses it to show only commands that exist. | — | — |
| `get build` | Shows which firmware build this is — board and role, as in the firmware file name, e.g. `heltec_v4_mn_rpt`; `unknown` when the firmware was not built with `build.sh`. The MeshNomad app uses it to offer the right file for this device. | — | — |

Example:

```
sensor scan
sensor read
get features
get build
```

### Network (ESP32)

MeshCore itself has WiFi only in its separate companion WiFi builds: the companion joins a
network and the app talks to it over TCP port 5000 (`wifi.ssid`, `wifi.pwd`, `wifi.clear`,
`wifi.enabled`, `wifi.status`, `wifi.ip`; applied after a reboot). MeshNomad keeps that
unchanged for the companion. Everything else here — WiFi for the repeater and room server
(same command names, applied immediately), the network console, MQTT with Home Assistant
discovery, NTP, the observer, `wifi.power` and `net …` — is this fork's own code
(`src/helpers/net/`), not part of MeshCore.

Roles: repeater, room server and companion on ESP32 boards. Everything here is off by default.
On the companion, WiFi itself (`wifi.ssid`, `wifi.pwd`, `wifi.enabled`, …) is MeshCore's own and
works as in MeshCore; this fork adds `wifi.power`, `mqtt.*`, `ntp.*`, `obs.*`, `net.telemetry`,
`net status` and `net apply`. The companion has no `tcp.enabled` and no `net scan`.
On the room server the network console (`tcp.enabled`) asks for the room server's admin password.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set wifi.ssid <name>` | WiFi network name. | text | empty |
| `get/set wifi.pwd <password>` | WiFi password. | text | empty |
| `set wifi.clear` | Empties the WiFi name and password. | — | — |
| `get/set wifi.enabled <0\|1>` | Turns WiFi on or off, immediately. On a repeater with `powersaving on`, WiFi keeps the board out of light sleep, which would drop the WiFi link; the room server and companion on ESP32 do not sleep, with or without WiFi. | `0`, `1` | `0` |
| `get wifi.status`, `get wifi.ip` | Connection state, IP address. | — | — |
| `get/set wifi.power <dBm>` | WiFi transmit power, applied immediately; `0` = chip default. | dBm | `0` |
| `get/set tcp.enabled <0\|1>` | Console over the network: TCP port 5000, asks for the node's admin password. Needs WiFi. Applied immediately. | `0`, `1` | `0` |
| `get/set mqtt.enabled <0\|1>` | Sends telemetry to an MQTT broker, with Home Assistant discovery. Needs WiFi and `mqtt.host`. | `0`, `1` | `0` |
| `get/set mqtt.host`, `mqtt.port`, `mqtt.user`, `mqtt.pwd`, `mqtt.topic` | Broker address, port, login, password, base topic. | text, number | empty, `1883`, empty, empty, `meshcore` |
| `get/set net.telemetry <seconds>` | How often telemetry is sent. | seconds | `60` |
| `get/set ntp.enabled <0\|1>` | Sets the node's clock from an NTP server. Needs WiFi. | `0`, `1` | `0` |
| `get/set ntp.server <host>`, `ntp.interval <seconds>` | NTP server; how often the clock is set again. | text, seconds | `pool.ntp.org`, `90000` |
| `get/set obs.enabled <0\|1>` | Observer: publishes every packet this node hears to MQTT, topic `<obs.topic>/<IATA>/<node key>/rx`. Needs MQTT and `obs.iata`. | `0`, `1` | `0` |
| `get/set obs.iata <code>` | Area code used in the observer topics, e.g. `WAW`. | text | empty |
| `get/set obs.topic <topic>` | Root of the observer topics, separate from `mqtt.topic`. | text, max 23 chars | `meshcore` |
| `net status` | One line: WiFi, IP, signal, MQTT, NTP, observer, packet counters. | — | — |
| `net scan` | Lists WiFi networks in range (takes 2–3 s). | — | — |
| `net apply` | Applies changed `mqtt.*`, `ntp.*`, `obs.*` settings without a reboot. | — | — |

Network console login (`tcp.enabled 1`): connect to port 5000 and type the node's admin
password (the one set with `password <new>`).

- The password must be typed within 15 seconds of connecting, otherwise the connection is closed.
- An empty admin password is never accepted — set one first.
- After 3 wrong passwords from one IP address, that address gets `Denied` for 5 minutes
  (each new attempt restarts the 5 minutes); other addresses can still log in. The node
  remembers up to 16 addresses.
- An idle logged-in session is closed after 120 seconds.

Example — join WiFi, open the network console, send telemetry, set the clock:

```
set wifi.ssid MyNetwork
set wifi.pwd MyPassword
set wifi.enabled 1
get wifi.ip
set tcp.enabled 1
set mqtt.host broker.example.org
set mqtt.enabled 1
set ntp.enabled 1
net apply
net status
```

## Supported boards

MeshNomad firmware is released only for ESP32 boards — the network features need WiFi —
and only where a board can hold the whole feature set of a role. Boards based on nRF52,
RP2040 or STM32 get no MeshNomad firmware, and neither does an ESP32 board for a role it
cannot hold; use the original [MeshCore firmware](https://github.com/meshcore-dev/MeshCore/releases)
there.

The MeshNomad app works with the original MeshCore firmware too: messaging, contacts,
channels and the administration of repeaters and room servers work as with any MeshCore
client. What such a node does not have is everything listed in [What you get](#what-you-get);
the app recognises a MeshCore build and does not offer those commands.

MeshNomad firmware is released for these boards (✓). The board name is the start of the
firmware file name, e.g. `Heltec_v3_mn_rpt-v1.18.0-meshnomad.1-<commit>.bin`; the vendor is
the name used to pick boards for development builds (see [Versions](#versions)). A — means
MeshCore releases no firmware of that role for the board, or, for the companion, the board
cannot hold it (see the table below).

<!-- >>> supported boards (generated by tool/meshnomad/gen_envs.py) -->
| Board | Vendor | Repeater | Companion | Room server |
|---|---|---|---|---|
| `Ebyte_EoRa-S3` | `ebyte` | ✓ | — | ✓ |
| `Generic_E22_sx1262` | `generic` | ✓ | — | — |
| `Generic_E22_sx1268` | `generic` | ✓ | — | — |
| `Heltec_ct62` | `heltec` | ✓ | ✓ | — |
| `Heltec_E213` | `heltec` | ✓ | ✓ | ✓ |
| `Heltec_E290` | `heltec` | ✓ | — | ✓ |
| `heltec_rc32` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_rc32_without_display` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_tracker_v2` | `heltec` | ✓ | ✓ | ✓ |
| `Heltec_v2` | `heltec` | ✓ | — | ✓ |
| `Heltec_v3` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_v4` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_v4_expansionkit` | `heltec` | ✓ | — | — |
| `heltec_v4_r8` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_v4_r8_tft` | `heltec` | ✓ | ✓ | ✓ |
| `heltec_v4_tft` | `heltec` | ✓ | ✓ | ✓ |
| `Heltec_Wireless_Paper` | `heltec` | ✓ | ✓ | ✓ |
| `Heltec_Wireless_Tracker` | `heltec` | ✓ | ✓ | ✓ |
| `Heltec_WSL3` | `heltec` | ✓ | ✓ | ✓ |
| `LilyGo_T3S3_sx1262` | `lilygo` | ✓ | — | ✓ |
| `LilyGo_T3S3_sx1276` | `lilygo` | ✓ | — | ✓ |
| `LilyGo_TBeam_1W` | `lilygo` | ✓ | — | ✓ |
| `LilyGo_TDeck` | `lilygo` | ✓ | ✓ | — |
| `LilyGo_TETH_Elite_sx1262` | `lilygo` | ✓ | ✓ | ✓ |
| `LilyGo_TLora_V2_1_1_6` | `lilygo` | ✓ | — | ✓ |
| `M5Stack_Unit_C6L` | `m5stack` | ✓ | — | ✓ |
| `Meshadventurer_sx1262` | `meshadventurer` | ✓ | — | ✓ |
| `Meshadventurer_sx1268` | `meshadventurer` | ✓ | — | ✓ |
| `meshnology_w12` | `meshnology` | ✓ | ✓ | ✓ |
| `RAK_3112` | `rak` | ✓ | ✓ | ✓ |
| `Station_G2` | `bq` | ✓ | — | ✓ |
| `Station_G2_logging` | `bq` | ✓ | — | — |
| `Station_G3_ESP32` | `bq` | ✓ | — | ✓ |
| `Station_G3_ESP32_logging` | `bq` | ✓ | — | — |
| `T_Beam_S3_Supreme_SX1262` | `lilygo` | ✓ | ✓ | ✓ |
| `Tbeam_SX1262` | `lilygo` | ✓ | — | ✓ |
| `Tbeam_SX1276` | `lilygo` | ✓ | — | ✓ |
| `Tenstar_C3_sx1262` | `tenstar` | ✓ | — | — |
| `Tenstar_C3_sx1268` | `tenstar` | ✓ | — | — |
| `ThinkNode_M2` | `elecrow` | ✓ | — | ✓ |
| `ThinkNode_M5` | `elecrow` | ✓ | — | ✓ |
| `ThinkNode_M7` | `elecrow` | ✓ | ✓ | ✓ |
| `Waveshare_S3_LR1121` | `waveshare` | ✓ | ✓ | ✓ |
| `Xiao_C3` | `seeed` | ✓ | ✓ | ✓ |
| `Xiao_S3` | `seeed` | ✓ | ✓ | ✓ |
| `Xiao_S3_WIO` | `seeed` | ✓ | ✓ | ✓ |
<!-- <<< supported boards -->

ESP32 boards MeshCore releases a companion for, but MeshNomad does not:

| Board | Why there is no MeshNomad companion |
|---|---|
| `Ebyte_EoRa-S3` | Bluetooth, USB, WiFi and the network layer do not fit in its flash |
| `Heltec_v2` | Bluetooth, USB, WiFi and the network layer do not fit in its RAM |
| `LilyGo_T3S3_sx1262` | do not fit in its flash |
| `LilyGo_T3S3_sx1276` | do not fit in its flash |
| `LilyGo_TBeam_1W` | do not fit in its flash |
| `LilyGo_TLora_V2_1_1_6` | do not fit in its RAM |
| `M5Stack_Unit_C6L` | do not fit in its flash |
| `Meshadventurer_sx1262` | do not fit in its RAM |
| `Meshadventurer_sx1268` | do not fit in its RAM |
| `Station_G2` | even MeshCore's own Bluetooth companion does not fit in its flash |
| `Station_G3_ESP32` | even MeshCore's own Bluetooth companion does not fit in its flash |
| `Tbeam_SX1262` | do not fit in its RAM |
| `Tbeam_SX1276` | do not fit in its RAM |
| `ThinkNode_M2` | do not fit in its flash |
| `ThinkNode_M5` | do not fit in its flash |

## Flashing

Every firmware comes as two files:

| File | Contains | Write it at | Use it to |
|---|---|---|---|
| `<board>_mn_<role>-<version>-merged.bin` | bootloader, partition table and firmware | `0x0` | install on a new board, move from other firmware, repair a board, or when the release notes say "Full reset required" |
| `<board>_mn_<role>-<version>.bin` | firmware only | `0x10000` | update a board that already runs MeshNomad or MeshCore firmware for the same board; also the file for over-the-air (OTA) updates |

The `-merged.bin` file also overwrites the `nvs` area, so Bluetooth pairings are lost and the
phone has to pair again. The node's identity key and contacts live in the file-system
partition, which neither file reaches (checked on Heltec V4; the layout is set per board).
A full chip erase (`esptool.py erase_flash`) before flashing removes them too.

With [esptool](https://github.com/espressif/esptool), using the chip of your board
(`esp32`, `esp32s3`, `esp32c3`, `esp32c6`):

```
esptool.py --chip esp32s3 write_flash 0x0 heltec_v4_mn_rpt-v1.18.0-meshnomad.1-9f3c2a1-merged.bin
esptool.py --chip esp32s3 write_flash 0x10000 heltec_v4_mn_rpt-v1.18.0-meshnomad.1-9f3c2a1.bin
```

Use esptool 4.8 or newer. Some boards connect through the chip's own USB, and the port
shows up as "USB JTAG/serial debug unit" (for example Heltec V4 and T-Beam S3 Supreme).
If an upload to such a board is interrupted, older versions cannot flash it again: the
chip resets during the erase, and the upload fails with
`Could not configure port: (5, 'Input/output error')`. `pio run -t upload` in this
repository already uses esptool 4.8.1.

Each release's notes say whether a board's partition table changed since the previous
release. If it did, that board needs the `-merged.bin` file at 0x0; otherwise the `.bin`
file at 0x10000 updates it and keeps its settings.

## Versions

| Tag | Firmware reports | Meaning |
|---|---|---|
| `rpt-v1.18.0-meshnomad.1` | `v1.18.0-meshnomad.1-<commit>` | first MeshNomad repeater release on MeshCore 1.18.0 |
| `cmp-v…`, `rs-v…` | same scheme | companion, room server |
| `dev-rpt-57`, `dev-cmp-57`, `dev-rs-57` (prerelease) | `v1.18.0-meshnomad.dev.57-<commit>` | development build no. 57 from `dev-meshnomad`, may be unstable; a higher number is a newer build; the newest 20 per role are kept. Never compare it with a release number. |

Development builds are started by hand: **Actions → Build Dev Firmware → Run workflow**
on `dev-meshnomad`, or `tool/meshnomad/dev_build.py` in a terminal. Boards are picked
from the two lists or as paths `<vendor>/<board>/<role>` — `heltec/heltec_v4/rpt`,
`heltec/heltec_v4` (all roles), `heltec`, `heltec/*/rpt`, `*/*/cmp`, several separated
by commas. Vendor and board names are the Vendor and Board columns of
[Supported boards](#supported-boards), in lower case.

## Building

Same as MeshCore — PlatformIO, see the [MeshCore documentation](https://github.com/meshcore-dev/MeshCore/tree/main/docs).
MeshNomad builds are the envs named `<board>_mn_<role>` (`rpt`, `cmp`, `rs`), e.g.
`pio run -e Heltec_v3_mn_rpt`. They are generated into `variants/meshnomad/platformio.ini`
by `tool/meshnomad/gen_envs.py`. MeshCore's own envs for ESP32 boards stay available
unchanged; the board variants without ESP32 are not part of this fork.

A plain `pio run` gives firmware that reports MeshCore's version (`v1.18.0`) and
`get build` → `unknown`, so the MeshNomad app treats it as MeshCore firmware. To build what
the releases contain, use `build.sh`; the two files of [Flashing](#flashing) land in `out/`:

```
FIRMWARE_VERSION=v1.18.0-meshnomad.local bash build.sh build-firmware Heltec_v3_mn_rpt
```

## Staying in sync

- `main` mirrors MeshCore's `main` and never carries fork changes.
- `main-meshnomad` is the fork's default branch and the only one releases are
  tagged from. It changes only through a pull request from `dev-meshnomad`, which
  is then fast-forwarded, so a release is built from exactly the commit the checks
  passed on.
- `dev-meshnomad` is where work happens: every change and every MeshCore release
  is merged there first, also through a pull request (from a `feat/`, `fix/` or
  `chore/` branch) that is fast-forwarded once its checks pass.
- After each MeshCore merge, run `tool/meshnomad/drop_unsupported.py`: it removes
  MeshCore's board variants and board files that have no ESP32 build (this also
  settles the merge conflicts on those files). Then run `tool/meshnomad/gen_envs.py`
  for the release envs. The pull request check fails if either step was missed.
- On a merge conflict in this `README.md`, keep this file.
