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
