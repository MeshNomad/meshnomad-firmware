# MeshNomad Firmware

Firmware for LoRa mesh radios used by the MeshNomad app — a fork of
[MeshCore](https://github.com/meshcore-dev/MeshCore) that stays as close to the
original as possible.

> **Not affiliated with the MeshCore project.** This is an independent fork. For
> MeshCore itself, its documentation, community and official releases, go to
> [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).

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
| Releases | Own release builds from `rpt-v…`, `cmp-v…`, `rs-v…` tags (repeater, companion, room server). The firmware reports its version as `v<MeshCore version>-meshnomad`, e.g. `v1.18.0-meshnomad`; the number after the dot in the tag (`rpt-v1.18.0-meshnomad.2`) only counts our releases. | 1.18.0 |
| Path hash | `path.hash.mode` defaults to `1` (2-byte path hash) on a fresh install; change it with `set path.hash.mode`. Existing settings are kept. | 1.18.0 |
| CLI | New commands: `sensor read` (live sensor values), `sensor scan` (I2C addresses on the sensor bus), `get features` (what this build contains, used by the MeshNomad app). | 1.18.0 |
| Network (ESP32 repeater) | Optional WiFi with a password-protected TCP console (port 5000), MQTT telemetry with Home Assistant discovery, SNTP clock sync and an MQTT packet observer. All of it is off by default and switched on with `set wifi.enabled 1`, `set tcp.enabled 1`, `set mqtt.enabled 1`, `set ntp.enabled 1`, `set obs.enabled 1`; settings via CLI (`wifi.*`, `tcp.enabled`, `mqtt.*`, `ntp.*`, `obs.*`, `net status|scan|apply`). | 1.18.0 |

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

### Network (ESP32 repeater)

Roles: repeater on ESP32 boards. Everything here is off by default.

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

## Versions

| Tag | Firmware reports | Meaning |
|---|---|---|
| `rpt-v1.18.0-meshnomad.1` | `v1.18.0-meshnomad-<commit>` | first MeshNomad repeater release on MeshCore 1.18.0 |
| `cmp-v…`, `rs-v…` | same scheme | companion, room server |

## Building

Same as MeshCore — PlatformIO, see the [MeshCore documentation](https://github.com/meshcore-dev/MeshCore/tree/main/docs).

## Staying in sync

- `main` mirrors MeshCore's `main` and never carries fork changes.
- `main-meshnomad` is the fork's default branch; MeshCore releases are merged
  into it.
- On a merge conflict in this `README.md`, keep this file.
