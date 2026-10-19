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
- **Network features:** WiFi, a password-protected network console, MQTT
  telemetry with Home Assistant discovery, NTP clock sync and an MQTT packet observer.
  All off until you switch them on.
- **Sensor and build commands:** `sensor read`, `sensor scan`, and `get features`, which
  lets the MeshNomad app show only the commands your firmware has.
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
| Releases | Own release builds from `rpt-v…`, `cmp-v…`, `rs-v…` tags (repeater, companion, room server). The firmware reports its version as `v<MeshCore version>-meshnomad.<release>`, e.g. `v1.18.0-meshnomad.2`; the number after the dot counts our releases on that MeshCore version and starts again at 1 on the next one. | 1.18.0 |
| Path hash | `path.hash.mode` defaults to `1` (2-byte path hash) on a fresh install; change it with `set path.hash.mode`. Existing settings are kept. | 1.18.0 |
| CLI | New commands: `sensor read` (live sensor values), `sensor scan` (I2C addresses on the sensor bus), `get features` (what this build contains, used by the MeshNomad app). | 1.18.0 |
| Network (ESP32) | Optional WiFi with a password-protected TCP console (port 5000), MQTT telemetry with Home Assistant discovery, SNTP clock sync and an MQTT packet observer. All of it is off by default and switched on with `set wifi.enabled 1`, `set tcp.enabled 1`, `set mqtt.enabled 1`, `set ntp.enabled 1`, `set obs.enabled 1`; settings via CLI (`wifi.*`, `tcp.enabled`, `mqtt.*`, `ntp.*`, `obs.*`, `net status|scan|apply`). | 1.18.0 |
| Firmware per role | One firmware per ESP32 board and role: repeater `<board>_mn_rpt`, companion `<board>_mn_cmp` (Bluetooth, USB and WiFi at the same time, with MQTT and the observer, all off by default). Boards without WiFi, and boards that cannot hold a role's full feature set, get no MeshNomad firmware — see [Supported boards](#supported-boards). Release builds have debug logging switched off. | 1.18.0 |

## Commands

Commands added by this fork, or whose default it changes. Type them on the USB
serial console, in the MeshNomad app console, or as remote admin over LoRa.
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

Roles: `sensor read` and `sensor scan` — repeater, room server; `get features` — all roles.

| Command | What it does | Values | Default |
|---|---|---|---|
| `sensor read` | Reads every sensor now and prints one value per line, channel first (`ch2 temp=21.3C`). | — | — |
| `sensor scan` | Lists the I2C addresses that answer on the sensor bus — a quick wiring check. Prints `no I2C devices` when nothing answers. | — | — |
| `get features` | Lists what this firmware build contains, e.g. `wifi mqtt ntp observer gps`; `none` when nothing extra. The MeshNomad app uses it to show only commands that exist. | — | — |

Example:

```
sensor scan
sensor read
get features
```

### Network (ESP32)

Roles: repeater and companion on ESP32 boards. Everything here is off by default.
On the companion, WiFi itself (`wifi.ssid`, `wifi.pwd`, `wifi.enabled`, …) is MeshCore's own and
works as in MeshCore; this fork adds `wifi.power`, `mqtt.*`, `ntp.*`, `obs.*`, `net.telemetry`,
`net status` and `net apply`. The companion has no `tcp.enabled` and no `net scan`.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set wifi.ssid <name>` | WiFi network name. | text | empty |
| `get/set wifi.pwd <password>` | WiFi password. | text | empty |
| `set wifi.clear` | Empties the WiFi name and password. | — | — |
| `get/set wifi.enabled <0\|1>` | Turns WiFi on or off, immediately. While WiFi is on the board does not sleep. | `0`, `1` | `0` |
| `get wifi.status`, `get wifi.ip` | Connection state, IP address. | — | — |
| `get/set wifi.power <dBm>` | WiFi transmit power, applied immediately; `0` = chip default. | dBm | `0` |
| `get/set tcp.enabled <0\|1>` | Console over the network: TCP port 5000, asks for the node's admin password. Needs WiFi. Applied immediately. | `0`, `1` | `0` |
| `get/set mqtt.enabled <0\|1>` | Sends telemetry to an MQTT broker, with Home Assistant discovery. Needs WiFi and `mqtt.host`. | `0`, `1` | `0` |
| `get/set mqtt.host`, `mqtt.port`, `mqtt.user`, `mqtt.pwd`, `mqtt.topic` | Broker address, port, login, password, base topic. | text, number | empty, `1883`, empty, empty, `meshcore` |
| `get/set net.telemetry <seconds>` | How often telemetry is sent. | seconds | `60` |
| `get/set ntp.enabled <0\|1>` | Sets the node's clock from an NTP server. Needs WiFi. | `0`, `1` | `0` |
| `get/set ntp.server <host>`, `ntp.interval <seconds>` | NTP server; how often the clock is set again. | text, seconds | `pool.ntp.org`, `90000` |
| `get/set obs.enabled <0\|1>` | Observer: publishes every packet this node hears to MQTT, topic `<topic>/<IATA>/<node key>/rx`. Needs MQTT and `obs.iata`. | `0`, `1` | `0` |
| `get/set obs.iata <code>` | Area code used in the observer topics, e.g. `WAW`. | text | empty |
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

Repeater: every ESP32 board MeshCore releases a repeater for.

Companion: every ESP32 board MeshCore releases a companion for, except:

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

## Versions

| Tag | Firmware reports | Meaning |
|---|---|---|
| `rpt-v1.18.0-meshnomad.1` | `v1.18.0-meshnomad.1-<commit>` | first MeshNomad repeater release on MeshCore 1.18.0 |
| `cmp-v…`, `rs-v…` | same scheme | companion, room server |

## Building

Same as MeshCore — PlatformIO, see the [MeshCore documentation](https://github.com/meshcore-dev/MeshCore/tree/main/docs).
MeshNomad builds are the envs named `<board>_mn_<role>` (`rpt`, `cmp`), e.g.
`pio run -e Heltec_v3_mn_rpt`. They are generated into `variants/meshnomad/platformio.ini`
by `tool/meshnomad/gen_envs.py`; MeshCore's own envs stay available unchanged.

## Staying in sync

- `main` mirrors MeshCore's `main` and never carries fork changes.
- `main-meshnomad` is the fork's default branch; MeshCore releases are merged
  into it.
- On a merge conflict in this `README.md`, keep this file.
