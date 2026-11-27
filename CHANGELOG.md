# Changelog

MeshNomad firmware releases. Each release is published three times, once per role, with the
tags `rpt-`, `cmp-` and `rs-` followed by the version (see
[Release channels](README.md#release-channels)). MeshCore's own changes are in the
[MeshCore releases](https://github.com/meshcore-dev/MeshCore/releases).

## v1.18.0-meshnomad.7 (2026-11-27)

Partition tables unchanged since `.4`: the `.bin` file at `0x10000` updates every board and
keeps its settings.

After this update a companion refuses every connection over WiFi (TCP) until a TCP password
is set over Bluetooth or USB (`set tcp.password`), and the app has to support the encrypted
link; `set tcp.auth 0` brings back the open link of MeshCore. Bluetooth and USB are not
affected.

### Highlights

- The companion's WiFi (TCP) link has a password and is encrypted: `tcp.auth` (on by
  default), `tcp.password`, `tcp.clear`; failed logins are locked out like on the repeater's
  console. `get features` reports `tcp.auth` and `tcp.enc`.
- WiFi guard on companions (`wifi.guard`, on by default): a link that stays connected but
  carries no traffic is reconnected, and after two failed reconnects the WiFi radio is
  restarted once. It was tested against a forced failure only.
- `region load` sent over LoRa is refused. In MeshCore, and in MeshNomad up to `.6`, it left a
  repeater or room server ignoring every command, `reboot` too, and the line that ended it
  replaced the region map.
- `wifi.power` now works on companions. In `.5` and `.6` the setting was stored but never
  reached the WiFi driver there; repeaters and room servers were not affected.
- A clock or contact times that are ahead are corrected as soon as the NTP server answers
  after a restart. In `.6` the correction ran only once per `ntp.interval` (25 hours by
  default), so in practice it needed `net ntp sync`.
- A cut or garbled frame no longer silences a companion's USB or WiFi link until a restart.
- `get mem` reports free RAM on every role; `gps` no longer crashes a node without a GPS
  module.

### Features

- WiFi guard restarts a companion link that carries no traffic
- Get mem reports the board's RAM on every role
- Encrypted companion TCP link (protocol v2)
- Password login for the companion TCP link

### Fixes

- Region load is refused from a remote sender and no longer locks the CLI
- Wifi.power reaches the WiFi driver on a companion
- Apply the NTP server time at the first reply after a restart
- Pull future contact times back at every NTP time
- A cut frame no longer silences the companion USB or TCP link
- Gps command no longer crashes nodes without a GPS module

### Refactoring

- Share the login lockout table

### Documentation

- Regions on a repeater and the region load lockout

## v1.18.0-meshnomad.6 (2026-11-15)

The tags `rpt-`, `cmp-` and `rs-v1.18.0-meshnomad.5` exist but have no release: the release
workflow could not start on them, and release tags cannot be moved. `.6` contains everything
that was meant for `.5`.

Partition tables unchanged since `.4`: the `.bin` file at `0x10000` updates every board and
keeps its settings.

### Highlights

- A clock that had moved into the future is corrected by NTP: once the NTP server has
  answered, its time is written even when the clock was more than an hour off. On a
  companion, contact times ahead of the corrected clock are pulled back to it, and at boot a
  set clock is no longer replaced by the time of the newest contact. Restart the node once
  after such a correction.
- `net ntp sync` sets the clock from the NTP server now; `get features` reports `ntp.sync`.
- A broker given as an IP address no longer delays the node's replies while it cannot be
  reached; Home Assistant discovery stops at the first failed message.
- `wifi.power` applies at once on every role, also back to `0` (chip default).
- A companion's `net status` reports `wifi:off` when WiFi was not started at boot.
- LilyGo T-Beam S3 Supreme: its 8 MB PSRAM is used. As a companion with WiFi it ran out of
  internal RAM after a boot, and WiFi could stop sending while still reporting
  `wifi:connected`.

### Features

- Add the net ntp sync command and its feature token

### Fixes

- Enable PSRAM on the T-Beam Supreme
- Apply wifi.power at once, back to chip default too, and only on change
- Connect to the MQTT broker without blocking the main loop
- Keep a set clock at boot and let a server NTP time correct it
- Stop the Home Assistant discovery burst at the first failed publish
- Report companion WiFi as off in net status when the driver is not running

### Documentation

- Link the changelog and note the host-name limit of mqtt.host

### CI

- Count release notes from the previous published release
- Run git-cliff from PyPI in the release job
- Release notes and changelog from git-cliff

### Chores

- Update github actions (#17)
- Update github/codeql-action action to v4.39.2 (#16)

## v1.18.0-meshnomad.4 (2026-11-03)

### Highlights

- Observer uplinks `obs1`..`obs3` to external brokers (LetsMesh, MeshMapper, regional and own
  brokers; MQTT, MQTTS, WS, WSS), off by default.

### Features

- Observer uplinks obs1..obs3 to external brokers

### Fixes

- Bounded appends in the observer uplink JSON and token
- Observer uplinks build on ESP-IDF 5 (ESP32-C6)

### Documentation

- Observer uplinks in the feature list, MeshCore comparison and known brokers

### Chores

- Update github actions (#14)
- Update github actions (#13)

## v1.18.0-meshnomad.3 (2026-11-01)

### Highlights

- WiFi on repeater and room server: three connect attempts with the radio off in between, then
  off until `net wifi retry`, so an absent network no longer keeps the radio on.
- Security policy, CodeQL, signed build provenance and OpenSSF Scorecard.

### Fixes

- Write the TLS certificate fingerprint by index instead of chained snprintf
- Stop WiFi after 3 failed attempts instead of scanning forever
- Probe INA3221 at 0x41 and 0x43

### Documentation

- Security policy with private reporting, scope and release file check
- README companion builds and WiFi compared with MeshCore precisely
- README compatibility matrix, remote management, narrower boards table
- MeshNomad README and logo, drop the MeshCore docs leftovers

### CI

- OpenSSF Scorecard
- Signed build provenance for release and dev firmware files
- CodeQL code scanning for C/C++
- Pin actions to commits, least-privilege tokens and a zizmor check

## v1.18.0-meshnomad.2 (2026-10-28)

### Highlights

- The network console on repeater and room server runs over TLS; it and `net ota` refuse
  logins while the admin password is still the factory default.
- `net ota`: firmware update over WiFi through an HTTPS page.

### Features

- Set refuses too-long network settings instead of cutting them
- Net ota, network OTA over HTTPS
- TLS network console with per-node certificate
- Board marker MNENV in the firmware image
- Hide WiFi and MQTT passwords in get replies
- Obs.topic, observer topic root separate from mqtt.topic

### Fixes

- Net ota survives browser connections (task watchdog, two TLS sockets, 30 s send timeout)
- Observer origin follows set name without a reboot
- Net CLI hook runs after the app's xx| prefix strip

### Documentation

- README for TLS console and net ota

### CI

- Drop Xiao_C3 repeater and room server from the PR build check
- Station G2 and G3 companions back with the 16 MB table
- Drop LilyGo TLora V2.1.1.6 repeater, the board leaves the release matrix
- Station G2 and G3 back with a 16 MB partition table
- Drop boards that cannot hold TLS from the release matrix

### Chores

- Add Renovate for GitHub Actions with digest pinning and OSV alerts
- Remove the MeshCore docs site, SECURITY.md and CONTRIBUTING.md

## v1.18.0-meshnomad.1 (2026-10-22)

### Highlights

- First MeshNomad release, on MeshCore 1.18.0, for ESP32 boards: repeater, companion and room
  server builds with WiFi, MQTT telemetry with Home Assistant discovery, NTP clock and a packet
  observer, all off by default.

### Features

- Fixed default Bluetooth PIN 123456 on the companion
- Build info on the repeater and room server boot screens
- MeshNomad logo, website and build info on the boot screen
- Get build reports the firmware build env
- Detect INA3221 and INA226 on alternate address 0x40
- One room server firmware per board
- One companion firmware per board
- One repeater firmware per board
- WiFi, TCP console, MQTT, SNTP and observer for the repeater
- Add sensor read, sensor scan and get features
- Default path.hash.mode to 1 in MeshNomad builds

### Fixes

- USB counts as connected only while the app sends frames
- Merged image keeps the bootloader flash mode
- Room server boot screen timed from UI start
- Install intelhex for the pinned esptool
- Esptool 4.8.1 for MeshNomad ESP32 uploads
- Name the repository in dev_build.py gh calls
- Match MeshCore role envs regardless of case
- Harden the TCP console login
- Build the network layer on ESP-IDF 5

### Refactoring

- Extract repeater stats and neighbour count accessors

### Documentation

- Where to type commands, flashing, vendor column, origin of the network layer
- Generated list of supported boards
- README for the MeshNomad firmware fork

### CI

- Run the pull request checks on pull requests to dev-meshnomad
- Note partition table changes in release notes
- One immutable dev prerelease per build
- Manual dev firmware builds with prereleases
- Run unit tests on pull requests only, skip docs-only changes
- Check pull requests only and keep unsupported board code out
- Build only MeshNomad envs in the PR check
- Compile the release counter into the firmware version
- Build and release MeshNomad firmware from rpt-, cmp- and rs- tags

### Chores

- Remove MeshCore project files the fork does not use
- Drop board variants MeshNomad does not support

