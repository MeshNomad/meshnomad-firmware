<p align="center">
  <img src="docs/assets/meshnomad-lockup.svg" alt="MeshNomad" width="160">
</p>

# MeshNomad Firmware

[![Licence: MIT](https://img.shields.io/badge/licence-MIT-blue)](license.txt)
[![Latest release](https://img.shields.io/github/v/release/MeshNomad/meshnomad-firmware)](https://github.com/MeshNomad/meshnomad-firmware/releases)

Firmware for ESP32 LoRa mesh radios used with the MeshNomad app. It is a fork of
[MeshCore](https://github.com/meshcore-dev/MeshCore) that keeps MeshCore's radio
protocol and routing untouched and adds network features on top.

Downloads: [Releases](https://github.com/MeshNomad/meshnomad-firmware/releases)
App: [MeshNomad app](https://github.com/MeshNomad/meshnomad)
Website: https://meshnomad.org

> MeshNomad is an independent fork and is not affiliated with the MeshCore project.
> For MeshCore itself, its documentation, community and releases, see
> [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).

Contents: [What MeshNomad is](#what-meshnomad-is) |
[Features by role](#features-by-role) | [Supported boards](#supported-boards) |
[Download and flash](#download-and-flash) | [First setup](#first-setup) |
[Network features](#network-features) | [Security notes](#security-notes) |
[Differences from MeshCore](#differences-from-meshcore) | [Commands](#commands) |
[Building from source](#building-from-source) | [Release channels](#release-channels) |
[Development](#development) | [Licence and credits](#licence-and-credits)

## What MeshNomad is

MeshNomad firmware is MeshCore with additions around the core. The radio layer, the
packet format and the routing are MeshCore's, so a MeshNomad node works in any MeshCore
mesh next to nodes running the original firmware. Every MeshCore release is merged into
this fork; nothing is sent back upstream.

The additions are built into every firmware and switched on at runtime, from the
MeshNomad app or the console. A freshly flashed node behaves like a MeshCore node with
two exceptions: it uses a 2-byte path hash by default, and a companion pairs with a fixed
Bluetooth PIN. Both can be changed.

The MeshNomad app also works with original MeshCore firmware. It detects which firmware
a node runs and offers only the commands that exist there.

Which client gets what:

| Firmware | Client | MeshCore features | MeshNomad features |
|---|---|---|---|
| MeshNomad | MeshNomad app | yes | yes |
| MeshNomad | other MeshCore apps | yes | CLI only |
| MeshNomad | console (CLI) | yes | yes |
| MeshCore | MeshNomad app | yes | no |
| MeshCore | other MeshCore apps | yes | no |
| MeshCore | console (CLI) | yes | no |

For installers, a repeater or room server on WiFi can be looked after from anywhere its
network reaches. The network console and `net ota` work from another subnet, over a VPN
for example, so a node on a mast or a hilltop that has a network there can be configured
and updated without going within range of its own access point.

## Features by role

One firmware per board and role. Everything below is off until you turn it on.

**Repeater** (`<board>_mn_rpt`) and **room server** (`<board>_mn_rs`)

- WiFi with a password-protected console over TLS on port 5000.
- Firmware update over the network from a browser (`net ota`).
- MQTT telemetry with Home Assistant discovery, NTP clock sync, and an MQTT packet
  observer that publishes every packet the node hears.
- Observer uplinks: the same packets to up to three public analyzers or other brokers at
  once (LetsMesh, MeshMapper, liteScope), with the signed login these analyzers use.
- `sensor read` and `sensor scan` for live sensor values and an I2C wiring check.

**Companion** (`<board>_mn_cmp`)

- Bluetooth, USB and WiFi in one firmware on every board; MeshCore ships separate
  builds (on a few boards its Bluetooth build also has WiFi, never USB).
- Pairs with PIN `123456` on every board. A PIN set with `set pin` or in the app
  still takes precedence.
- WiFi settings and the app connection over WiFi work exactly as in MeshCore: the
  settings apply after a reboot and the app talks to the companion on port 5000.
- MQTT telemetry, NTP and the observer as on the other roles. No network console, no
  `net ota` and no observer uplinks: the companion is updated over USB.
- Its USB port speaks the app protocol, not text, so there is no serial console. Commands
  reach a companion from the app, or as remote admin over LoRa from a contact you allowed
  remote CLI.

**All roles**

- `get features` tells the app which features this build contains; `get build` tells it
  which firmware file fits the device.
- 2-byte path hash by default on a fresh install (`set path.hash.mode` changes it).
- INA3221 and INA226 current monitors are also found on I2C address 0x40 (identified by
  their chip ID, so an INA219 on 0x40 still works); INA3221 also on 0x41 and 0x43, so
  any A0 strapping works.

## Supported boards

MeshNomad firmware is released for ESP32 boards, and only where a board can hold the
full feature set of a role. The TLS console and `net ota` make the repeater and room
server images larger, and the companion carries Bluetooth, USB, WiFi and the network
layer at once; a board whose flash or RAM cannot hold that gets no MeshNomad firmware
for the role. Boards based on nRF52, RP2040 or STM32 get none at all. For every other
board, and for the roles marked `no` below, use the original
[MeshCore firmware](https://github.com/meshcore-dev/MeshCore/releases); the MeshNomad
app works with it too.

The board name is the start of the firmware file name; the vendor is used to pick boards
for development builds. RPT = repeater, CMP = companion, RS = room server, the same
abbreviations as in the file names (`_mn_rpt`, `_mn_cmp`, `_mn_rs`).

<!-- >>> supported boards (generated by tool/meshnomad/gen_envs.py) -->
| Board | Vendor | RPT | CMP | RS |
|---|---|---|---|---|
| `Heltec_ct62` | `heltec` | yes | yes | no |
| `Heltec_E213` | `heltec` | yes | yes | yes |
| `Heltec_E290` | `heltec` | yes | no | yes |
| `heltec_rc32` | `heltec` | yes | yes | yes |
| `heltec_rc32_without_display` | `heltec` | yes | yes | yes |
| `heltec_tracker_v2` | `heltec` | yes | yes | yes |
| `Heltec_v2` | `heltec` | yes | no | yes |
| `Heltec_v3` | `heltec` | yes | yes | yes |
| `heltec_v4` | `heltec` | yes | yes | yes |
| `heltec_v4_expansionkit` | `heltec` | yes | no | no |
| `heltec_v4_r8` | `heltec` | yes | yes | yes |
| `heltec_v4_r8_tft` | `heltec` | yes | yes | yes |
| `heltec_v4_tft` | `heltec` | yes | yes | yes |
| `Heltec_Wireless_Paper` | `heltec` | yes | yes | yes |
| `Heltec_Wireless_Tracker` | `heltec` | yes | yes | yes |
| `Heltec_WSL3` | `heltec` | yes | yes | yes |
| `LilyGo_TDeck` | `lilygo` | yes | yes | no |
| `LilyGo_TETH_Elite_sx1262` | `lilygo` | yes | yes | yes |
| `M5Stack_Unit_C6L` | `m5stack` | yes | no | yes |
| `Meshadventurer_sx1262` | `meshadventurer` | yes | no | yes |
| `Meshadventurer_sx1268` | `meshadventurer` | yes | no | yes |
| `meshnology_w12` | `meshnology` | yes | yes | yes |
| `RAK_3112` | `rak` | yes | yes | yes |
| `Station_G2` | `bq` | yes | yes | yes |
| `Station_G3_ESP32` | `bq` | yes | yes | yes |
| `T_Beam_S3_Supreme_SX1262` | `lilygo` | yes | yes | yes |
| `Tbeam_SX1262` | `lilygo` | yes | no | yes |
| `Tbeam_SX1276` | `lilygo` | yes | no | yes |
| `ThinkNode_M7` | `elecrow` | yes | yes | yes |
| `Waveshare_S3_LR1121` | `waveshare` | yes | yes | yes |
| `Xiao_C3` | `seeed` | no | yes | no |
| `Xiao_S3` | `seeed` | yes | yes | yes |
| `Xiao_S3_WIO` | `seeed` | yes | yes | yes |
<!-- <<< supported boards -->

## Download and flash

Every release ships two files per board and role:

| File | Contains | Write it at |
|---|---|---|
| `<board>_mn_<role>-<version>-merged.bin` | bootloader, partition table and firmware | `0x0` |
| `<board>_mn_<role>-<version>.bin` | firmware only | `0x10000` |

**New board, or a move from other firmware.** Write the `-merged.bin` file at `0x0`
with [esptool](https://github.com/espressif/esptool), using the chip of your board
(`esp32`, `esp32s3`, `esp32c3`, `esp32c6`):

```
esptool.py --chip esp32s3 write_flash 0x0 <board>_mn_<role>-<version>-merged.bin
```

The `-merged.bin` file also overwrites the `nvs` area, so Bluetooth pairings are lost and
the phone has to pair again. The node's identity key and contacts live in the file-system
partition, which neither file touches. A full chip erase (`esptool.py erase_flash`)
removes them too.

**Updating a node that already runs MeshNomad.** Two ways keep the node's identity,
contacts and settings:

- `net ota` in a browser (repeater and room server): upload the `.bin` file on the
  node's update page. See [Network firmware update](#network-firmware-update).
- The `-merged.bin` file at `0x0` over USB, for every role.

The plain `.bin` at `0x10000` also works, but only on a node that has never been updated
over the air: after an OTA update the node starts from its second firmware slot, and a
file written at `0x10000` is ignored. When in doubt, use one of the two ways above.

Each release's notes say whether a board's partition table changed since the previous
release. If it did, that board needs the `-merged.bin` file once; `net ota` refuses an
image that does not fit the firmware slot.

`Station_G2` and `Station_G3_ESP32` use a 16 MB partition table. Moving to the first
MeshNomad release with that table needs a USB flash of the `-merged.bin` at `0x0`; the node
starts fresh (new identity, settings and certificate), and nothing can preserve the old
ones.

Use esptool 4.8 or newer. Some boards connect through the chip's own USB, and the port
shows up as "USB JTAG/serial debug unit" (for example Heltec V4 and T-Beam S3 Supreme).
If an upload to such a board is interrupted, older esptool versions cannot flash it
again: the chip resets during the erase and the upload fails with
`Could not configure port: (5, 'Input/output error')`.

## First setup

The steps below use the console: the USB serial port on a repeater or room server, the
MeshNomad app's node console, or remote admin over LoRa. `get <key>` shows a value,
`set <key> <value>` changes and saves it.

1. Set the admin password. The network console and `net ota` refuse to start while the
   password is empty or still the factory default.

       password <new-password>

2. Join a WiFi network. WiFi starts at once on a repeater or room server; a companion
   needs a reboot.

       set wifi.ssid <network>
       set wifi.pwd <password>
       set wifi.enabled 1
       get wifi.ip

3. Open the network console (repeater and room server). It speaks TLS only, so plain
   `nc` or telnet does not connect.

       set tcp.enabled 1

   Then, from your computer:

       openssl s_client -quiet -connect <ip>:5000
       ncat --ssl <ip> 5000

   Type the admin password at the prompt. After the node closes the session,
   `openssl s_client -quiet` keeps waiting; press Enter or Ctrl+C.

4. Check that you are talking to your node. Each node generates its own key and
   self-signed certificate on the first boot with WiFi on. Compare the fingerprint the
   node reports with the one your computer sees:

       get net.cert
       openssl s_client -connect <ip>:5000 </dev/null | openssl x509 -outform DER | sha256sum

   `net cert renew` generates a new key and certificate.

Login rules, shared by the console and the `net ota` page: the password must arrive
within 15 seconds of connecting; after three failures from one address that address is
denied for five minutes (each new attempt restarts the five minutes); a failed TLS
handshake counts as a failure; an idle session is closed after 120 seconds.

From here, continue with [Network features](#network-features) to turn on MQTT, NTP,
the observer, observer uplinks or a network firmware update.

## Network features

All of them need WiFi (step 2 of [First setup](#first-setup)). Switches take effect
without a reboot on a repeater or room server; on a companion, WiFi itself needs a reboot
and the rest follows `net apply`.

### MQTT and Home Assistant

`set mqtt.host <broker>` and `set mqtt.enabled 1` start telemetry: battery, uptime, free
heap, neighbour count, packet counters, last RSSI and SNR, noise floor, and the values of
connected sensors (temperature, humidity, pressure, up to three current channels). The
node publishes to `<mqtt.topic>/<node id>/state` every `net.telemetry` seconds, keeps
`<mqtt.topic>/<node id>/status` at `online` or `offline`, and announces its sensors
through Home Assistant discovery, so they appear in Home Assistant without manual setup.

### Clock from NTP

`set ntp.enabled 1` sets the node's clock from `ntp.server` (UTC) and refreshes it every
`ntp.interval` seconds, so a node without a GPS keeps the right time across reboots.

### Packet observer

`set obs.iata <code>` and `set obs.enabled 1` publish every packet the node hears to
`<obs.topic>/<area code>/<node key>/rx` as raw hex with SNR and RSSI, plus a status
message with the node name, model and firmware version. The topic root is separate from
`mqtt.topic`, so an observer can feed a public packet collector while telemetry stays on
your own broker. Needs MQTT.

### Observer uplinks

A repeater or room server can also send the packets it hears to up to three external
brokers at once, `obs1` to `obs3`, next to your own broker: public analyzers such as
LetsMesh, MeshMapper or liteScope. Each uplink is empty after install and off until you
set it; the firmware ships no broker addresses. An uplink takes a URL (`mqtt://`,
`mqtts://`, `ws://`, `wss://`), a user and password or a signed login token (`obsN.aud`),
and publishes to `meshcore/<area code>/<node key>/packets` with the meshcoretomqtt JSON
used by these analyzers (or the short `/rx` format), plus a status message. The token is
signed on the node with its own key (login `v1_<node key>`, valid 24 h) and needs the
clock, so set NTP first. At most two TLS uplinks (`mqtts://`, `wss://`) run at once; each
needs about 60 KB of memory. A `net ota` session pauses them. Uplinks publish only:
nothing from a broker reaches the mesh. Not in the companion.

Example, LetsMesh EU and MeshMapper:

```
set obs.iata WAW
set obs1.host wss://mqtt-eu-v1.letsmesh.net:443/mqtt
set obs1.aud mqtt-eu-v1.letsmesh.net
set obs1.on 1
set obs2.host wss://mqtt.meshmapper.net:443/mqtt
set obs2.aud mqtt.meshmapper.net
set obs2.on 1
get obs.status
```

Known brokers, checked on 2026-11-02. They are run by others and may change or close;
the firmware does not depend on any of them. Set `obs.iata` to the IATA code of the
nearest airport (not ICAO): LetsMesh corrects invalid codes.

| Broker | `obsN.host` | Login |
|---|---|---|
| LetsMesh EU | `wss://mqtt-eu-v1.letsmesh.net:443/mqtt` | `obsN.aud mqtt-eu-v1.letsmesh.net` |
| LetsMesh US | `wss://mqtt-us-v1.letsmesh.net:443/mqtt` | `obsN.aud mqtt-us-v1.letsmesh.net` |
| MeshMapper | `wss://mqtt.meshmapper.net:443/mqtt` | `obsN.aud mqtt.meshmapper.net` |
| meshcore.ca | `wss://mqtt1.meshcore.ca:443/mqtt` | `obsN.aud mqtt1.meshcore.ca` |
| liteScope (litescope.riko.dev) | `mqtt://mqtt.riko.dev:1883` | `obsN.user litescope`, `obsN.pwd meshcore` (published on the liteScope observer page), no TLS |
| Your own broker or liteScope | `mqtt://<host>:1883` or `mqtts://<host>:8883` | its user and password |

A TLS broker must have a certificate from Let's Encrypt (ISRG Root X1) or Google Trust
Services (GTS Root R4); others fail with `tls` in `get obsN.diag`.

### Network console

`set tcp.enabled 1` opens the TLS console on port 5000 (repeater and room server). It
takes the same commands as the serial console and asks for the admin password first;
see [First setup](#first-setup) for the connection commands and the login rules. One
session at a time. The TLS handshake takes a few seconds on an ESP32, and the node
pauses mesh work for that time.

### Network firmware update

`net ota start` (repeater and room server) serves an update page at `https://<ip>/update`
and replies with that address. In the browser:

- The certificate is the node's own self-signed one, so the browser warns once per node.
- The first page load takes several seconds: the TLS handshake runs in software on the
  ESP32. The browser may open two connections; the node allows two at most.
- The page asks for the admin password and the `<board>_mn_<role>-<version>.bin` file.
  A wrong password counts towards the lockout shared with the console.
- A failed or interrupted upload never reboots the node; the old firmware stays. After a
  successful upload the page says so and the node restarts into the new firmware.

The session ends after `ota.timeout` seconds without data (600 by default, `0` means no
limit); `net ota status` shows the time left and the upload progress, `net ota stop` ends
it. Running `net ota start` again while a session runs only restarts the timeout.
`net scan`, `net apply` and `set wifi.enabled 0` end a running session first.

MeshCore's own `start ota` is unchanged and still available: it opens the `MeshCore-OTA`
access point. It is refused while WiFi is on or while a `net ota` session runs.

## Security notes

To report a vulnerability or to verify a downloaded firmware file, see
[SECURITY.md](SECURITY.md).

- The network console speaks TLS only; there is no plain-text mode. Each node generates
  its own ECDSA P-256 key and a self-signed certificate (CN = node id, O = node name) on
  the first boot with WiFi on. `get net.cert` shows the SHA-256 fingerprint for a manual
  check; `net cert renew` replaces the key and certificate.
- The console and `net ota` refuse every login while the admin password is empty or the
  factory default, and share one lockout table (the login rules in
  [First setup](#first-setup) apply to both).
- `get wifi.pwd` and `get mqtt.pwd` reply `(set)` or `(not set)`, never the password.
  As in MeshCore, the reply to `password <new>` echoes the new admin password and
  `get guest.password` shows the guest password.
- MeshCore's `start ota` opens an access point without a password, as in the original
  firmware. Prefer `net ota`, which stays on your WiFi network and asks for the admin
  password over TLS.
- `set` refuses a text value longer than its field (`Error, max <n> chars`) instead of
  cutting it, so a stored WiFi or MQTT password is always exactly what you typed.

## Differences from MeshCore

| Area | MeshCore | MeshNomad |
|---|---|---|
| Companion firmware | separate Bluetooth, USB and WiFi builds; on 9 boards the Bluetooth build also has WiFi | one build with Bluetooth, USB and WiFi on every board |
| Boards | ESP32, nRF52, RP2040, STM32 | ESP32 only, see [Supported boards](#supported-boards) |
| Path hash | 1 byte (`path.hash.mode 0`) | 2 bytes (`path.hash.mode 1`) on a fresh install |
| Companion Bluetooth PIN | random at every start on boards with a display | `123456` everywhere; `set pin` overrides |
| WiFi | companion: app connection; repeater and room server: only the `start ota` access point | all roles join your network; repeater and room server apply it at once |
| Network console | none | TLS on port 5000, admin password, repeater and room server |
| Firmware update over WiFi | `start ota` access point | `net ota` from a browser, plus `start ota` unchanged |
| MQTT, NTP, observer | none | all roles, off by default |
| Packets to public analyzers | a host computer with meshcoretomqtt next to the node | observer uplinks on the node itself: up to 3 brokers, signed login, repeater and room server, off by default |
| Password replies | `get wifi.pwd` shows the password | `(set)` or `(not set)` for `wifi.pwd`, `mqtt.pwd` and `obsN.pwd` |
| Sensor commands | `sensor get`, `sensor set` | plus `sensor read`, `sensor scan` |
| Build information | `ver` | plus `get features`, `get build` |
| Current monitors | INA226 and INA3221 on their default addresses | also found on 0x40; INA3221 on any address 0x40 to 0x43 |
| Debug logging | per build | off in every release build |

## Commands

Commands added by this fork, or whose default it changes. Everything else is MeshCore's;
see MeshCore's
[CLI reference](https://github.com/meshcore-dev/MeshCore/blob/main/docs/cli_commands.md).
These tables will move to `docs/cli.md` later.

Where to type them: the USB serial console of a repeater or room server, the network
console, the MeshNomad app's node console, or remote admin over LoRa. `get <key>` shows
the current value, `set <key> <value>` changes and saves it. Every reply starts with `> `;
a refused value starts with `Error`.

### Path hash

Roles: repeater, room server, companion.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get path.hash.mode` | How many bytes of each node's hash this node writes into the path of the packets it sends. | | |
| `set path.hash.mode <n>` | `0` = 1 byte, `1` = 2 bytes, `2` = 3 bytes. Longer hashes tell nodes apart more reliably; shorter ones leave room for longer paths. A node with a saved setting keeps it. | `0`, `1`, `2` | `1` (MeshCore: `0`) |

### Sensors and build information

Roles: `sensor read` and `sensor scan` on repeater and room server; `get features` and
`get build` on all roles.

| Command | What it does | Example reply |
|---|---|---|
| `sensor read` | Reads every sensor now, one value per line, channel first. `no sensor data` when there is none. | `ch2 temp=21.3C` |
| `sensor scan` | Lists the I2C addresses that answer on the sensor bus. | `> 2: 0x40 0x76`, `> no I2C devices` |
| `get features` | What this build contains; the app shows only the matching commands. A repeater or room server reports `tls`, `ota.net` and `obsup` (observer uplinks), a companion does not; `gps` appears on boards with a GPS. | `> wifi mqtt ntp observer tls ota.net obsup` |
| `get build` | Board and role as in the firmware file name; `unknown` when the firmware was not built with `build.sh`. The app uses it to offer the right file. | `> heltec_v4_mn_rpt` |

### WiFi

Roles: repeater, room server, companion. On the companion these are MeshCore's own
commands and apply after a reboot; on the other roles they apply at once.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set wifi.ssid <name>` | Network name. | text, max 32 chars | empty |
| `get/set wifi.pwd <password>` | Network password; `get` replies `(set)` or `(not set)`. | text, max 64 chars | empty |
| `set wifi.clear` | Empties the network name and password. | | |
| `get/set wifi.enabled <0\|1>` | WiFi on or off. On a repeater with `powersaving on`, WiFi keeps the board awake; the room server and companion on ESP32 do not sleep anyway. Repeater and room server: after a boot or a lost link the node makes 3 connect attempts (radio off 15 s, then 30 s in between); if all fail, the WiFi radio stays off until `net wifi retry`, `net apply`, `set wifi.ssid`, `set wifi.pwd`, `set wifi.enabled 1` or a reboot. | `0`, `1` | `0` |
| `get wifi.status`, `get wifi.ip` | `connected` or `disconnected`; the IP address or `(not connected)`. | | |
| `get/set wifi.power <dBm>` | Transmit power, applied at once; `0` = chip default. | `0` to `20` | `0` |
| `net scan` | Lists networks in range with their signal, as the board sees them (takes 2 to 3 seconds). Repeater and room server. A scan drops the WiFi link; during a connect attempt it starts the attempts again at 1/3, and within 30 s of connecting it uses up one attempt. | | |
| `net wifi retry` | Starts the 3 connect attempts again, for example after the attempts failed. Repeater and room server. | | |
| `net status` | One line: WiFi, IP, signal, MQTT, NTP, observer, observer packet counters. WiFi on the repeater and room server: `off`, `no ssid`, `try <n>/3(wl=<code>)`, `wait <n>/3` (radio off between attempts), `failed` (radio off, see `net wifi retry`), `connected`. | | |
| `net apply` | Applies changed `mqtt.*`, `ntp.*` and `obs.*` settings without a reboot (uplink settings `obsN.*` apply at once anyway). | | |

### Network console and certificate

Roles: repeater, room server.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set tcp.enabled <0\|1>` | Console over TLS on port 5000; asks for the admin password. Needs WiFi and a non-default admin password. | `0`, `1` | `0` |
| `get net.cert` | SHA-256 fingerprint of the node's certificate, `> sha256:<hex>`. Before the WiFi radio has been on once: `Error: no certificate yet, ...` with a hint (`set wifi.enabled 1 first`, `set wifi.ssid first` or `net wifi retry`). | | |
| `net cert renew` | New key and certificate; replies with the new fingerprint. Needs WiFi on. An open console session stays on the old certificate until it disconnects. | | |

### Firmware update over the network

Roles: repeater, room server.

| Command | What it does | Values | Default |
|---|---|---|---|
| `net ota start` | Starts the update page: `> https://<ip>/update (600 s)`, or `(no limit)`. Refused with `Error: WiFi not connected`, `Error: change the admin password first (password <new>)`, `Error: not enough memory` or `Error: MeshCore OTA access point active, reboot first`. | | |
| `net ota stop` | Ends the session: `> net ota stopped`, or `> off` when none runs. An upload in progress is aborted without a reboot. | | |
| `net ota status` | `> off`, `> on https://<ip>/update left:<seconds or -> upload:<bytes or idle>`, `> stopping` or `> update done, restarting`. | | |
| `get/set ota.timeout <s>` | How long a session waits without data before it ends; `0` = no limit. | `0` to `86400` | `600` |

### MQTT settings

Roles: repeater, room server, companion.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set mqtt.enabled <0\|1>` | Telemetry to an MQTT broker with Home Assistant discovery. Needs WiFi and `mqtt.host`. | `0`, `1` | `0` |
| `get/set mqtt.host <host>` | Broker address. | text, max 63 chars | empty |
| `get/set mqtt.port <port>` | Broker port. | `1` to `65535` | `1883` |
| `get/set mqtt.user <name>` | Broker login. | text, max 32 chars | empty |
| `get/set mqtt.pwd <password>` | Broker password; `get` replies `(set)` or `(not set)`. | text, max 64 chars | empty |
| `get/set mqtt.topic <topic>` | Base topic for telemetry. | text, max 23 chars | `meshcore` |
| `get/set net.telemetry <s>` | How often telemetry is sent. | `10` and up | `60` |

### NTP settings

Roles: repeater, room server, companion.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set ntp.enabled <0\|1>` | Sets the node's clock from an NTP server. Needs WiFi. | `0`, `1` | `0` |
| `get/set ntp.server <host>` | NTP server. | text, max 63 chars | `pool.ntp.org` |
| `get/set ntp.interval <s>` | How often the clock is set again. | `60` and up | `90000` |

### Observer settings

Roles: repeater, room server, companion.

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set obs.enabled <0\|1>` | Publishes every packet this node hears to `<obs.topic>/<obs.iata>/<node key>/rx`. Needs MQTT and `obs.iata`. | `0`, `1` | `0` |
| `get/set obs.iata <code>` | Area code used in the observer topics, stored in upper case. | text, max 7 chars | empty |
| `get/set obs.topic <topic>` | Root of the observer topics, separate from `mqtt.topic`. | text, max 23 chars | `meshcore` |

### Observer uplink settings

Roles: repeater, room server. `N` is 1, 2 or 3. `-` clears a text value. Changes apply at
once (the uplink reconnects).

| Command | What it does | Values | Default |
|---|---|---|---|
| `get/set obsN.on <0\|1>` | Turns the uplink on or off. Refused without `obs.iata` or `obsN.host`, and for a third TLS uplink. Off sends a final `offline` status. | `0`, `1` | `0` |
| `get/set obsN.host <url>` | Broker URL; a bare host means `mqtt://<host>:<obsN.port>`. | `mqtt://`, `mqtts://`, `ws://`, `wss://`, max 95 chars | empty |
| `get/set obsN.port <port>` | Port for a bare host. | `0`-`65535`, `0` = 1883 | `0` |
| `get/set obsN.user <name>` | Broker user name (not used with a token). | text, max 32 chars | empty |
| `get/set obsN.pwd <password>` | Broker password; `get` shows only whether it is set. | text, max 64 chars | empty |
| `get/set obsN.aud <host>` | Set: log in with a token signed by the node (user `v1_<node key>`, audience = this value, usually the broker host). | text, max 63 chars | empty |
| `get/set obsN.format <packets\|rx>` | `packets`: meshcoretomqtt JSON on `/packets`; `rx`: the short observer JSON on `/rx`. | `packets`, `rx` | `packets` |
| `get/set obsN.topic <template>` | Topic template; `{iata}`, `{key}` and `{type}` are replaced. | text, max 47 chars | `meshcore/{iata}/{key}/{type}` |
| `get/set obs.owner <key>` | Optional owner public key in the token. | 64 hex chars | empty |
| `get/set obs.email <address>` | Optional owner e-mail in the token. | text, max 63 chars | empty |
| `get obs.status` | State and packet counters of the three uplinks. | | `> obs1:conn pub:12 drop:0 obs2:off ...` |
| `get obsN.diag` | One uplink: state, last error (`connect`, `tls`, `auth`, `heap`, `timeout`, `closed`), tries, seconds to the next try. | | |

States: `off`, `wait wifi`, `wait clock` (token needs the clock), `wait heap` (not enough
memory for a TLS session: retried every 30 s ten times, then every 5 min ten times, then
every 10 min ten times, then every 30 min), `wait turn` (one TLS handshake at a time),
`retry`, `connecting`, `conn`, `paused` (`net ota` session).

Example session on a repeater: join WiFi, open the network console, send telemetry, set
the clock.

```
set wifi.ssid <network>
set wifi.pwd <password>
set wifi.enabled 1
get wifi.ip
set tcp.enabled 1
set mqtt.host <broker>
set mqtt.enabled 1
set ntp.enabled 1
net apply
net status
```

## Building from source

The build is MeshCore's: [PlatformIO](https://platformio.org/), the same `platformio.ini`
and board variants. MeshNomad firmware is the envs named `<board>_mn_<role>` with `rpt`,
`cmp` or `rs`, generated into `variants/meshnomad/platformio.ini` by
`tool/meshnomad/gen_envs.py`; each one extends the unchanged MeshCore env of that board.
MeshCore's own envs for ESP32 boards stay available; the board variants without an ESP32
build are not part of this fork.

```
pio run -e <board>_mn_<role>
```

A plain `pio run` gives a firmware that reports MeshCore's version and answers `get build`
with `unknown`, so the app treats it as MeshCore firmware. To build what the releases
contain, use `build.sh` with a version string; the two files from
[Download and flash](#download-and-flash) land in `out/`:

```
FIRMWARE_VERSION=v<meshcore>-meshnomad.local bash build.sh build-firmware <board>_mn_<role>
```

Release builds have MeshCore's debug logging switched off. `pio run -t upload` uses
esptool 4.8.1, which handles boards connected through the chip's own USB.

## Release channels

| Channel | Tag | Firmware reports |
|---|---|---|
| Release | `rpt-v<meshcore>-meshnomad.<n>`, the same with `cmp-` and `rs-` | `v<meshcore>-meshnomad.<n>-<commit>` |
| Development (prerelease) | `dev-rpt-<build>`, `dev-cmp-<build>`, `dev-rs-<build>` | `v<meshcore>-meshnomad.dev.<build>-<commit>` |

`<meshcore>` is the MeshCore version the release is built on; `<n>` counts MeshNomad
releases on that MeshCore version and starts again at 1 on the next one. For example,
`rpt-v1.18.0-meshnomad.1` is the first MeshNomad repeater release on MeshCore 1.18.0.

Development builds come from the `dev-meshnomad` branch, one prerelease per role and run;
a higher build number is a newer build, and the newest 20 per role are kept. They may be
unstable and their build number says nothing about release numbers. They are started by
hand from the Actions tab (Build Dev Firmware) or with `tool/meshnomad/dev_build.py`,
which picks boards as `<vendor>/<board>/<role>` paths such as `heltec/heltec_v4/rpt`,
`heltec`, `heltec/*/rpt` or `*/*/cmp`, with vendor and board names from
[Supported boards](#supported-boards) in lower case.

## Development

- `main` mirrors MeshCore's `main` and never carries fork changes. `main-meshnomad` is the
  default branch and the only one releases are tagged from; it changes only through a
  pull request from `dev-meshnomad`, where all work and every MeshCore merge land first.
- After a MeshCore merge, run `tool/meshnomad/drop_unsupported.py` (removes the board
  variants that have no ESP32 build) and then `tool/meshnomad/gen_envs.py` (release envs,
  the boards table above and the dev-build targets). The pull request check fails if
  either step was missed.
- Every MeshNomad change inside a MeshCore file, and every MeshCore file this fork
  deletes, is listed in [`docs/UPSTREAM-HUNKS.md`](docs/UPSTREAM-HUNKS.md).
- MeshCore's protocol and CLI documentation is not copied here; read it in
  [MeshCore's `docs/`](https://github.com/meshcore-dev/MeshCore/tree/main/docs).

## Licence and credits

MIT, unchanged from MeshCore; see [`license.txt`](license.txt). MeshCore is the work of
Scott Powell and the MeshCore contributors; all MeshCore code keeps its original
copyright. The MeshNomad additions are released under the same licence.
