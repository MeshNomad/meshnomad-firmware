# Security policy

## Reporting a vulnerability

Report security problems privately through GitHub:
[Report a vulnerability](https://github.com/MeshNomad/meshnomad-firmware/security/advisories/new).
Please do not open a public issue, pull request or discussion for them.

Useful details: the firmware version (`ver`), the board and role, the steps to reproduce,
and what an attacker gains (for example console access, a firmware update without the
admin password, or reading the WiFi or MQTT password).

MeshNomad is maintained by a small team in spare time. A fix for a confirmed problem goes
into the next release and is published as an advisory on this repository once the fixed
firmware is available.

## Scope

| Where the problem is | Where to report it |
|---|---|
| MeshNomad additions: network console, `net ota`, WiFi, MQTT, NTP, observer, sensors, build and release workflows | here |
| MeshCore code that MeshNomad uses unchanged (radio, packet format, routing, encryption, `start ota`) | [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore); a note here as well is welcome, so the fix reaches MeshNomad |
| MeshNomad app | [MeshNomad/meshnomad](https://github.com/MeshNomad/meshnomad) |

Known behaviour that is not a vulnerability is described in the README under
[Security notes](README.md#security-notes).

## Supported versions

| Version | Security fixes |
|---|---|
| Latest release of each role (`rpt-`, `cmp-`, `rs-v<meshcore>-meshnomad.<n>`) | yes |
| Older releases | no, update to the latest release |
| Development builds (`dev-rpt-`, `dev-cmp-`, `dev-rs-<build>`) | no |

## Verifying release files

Firmware files are built by GitHub Actions in this repository. From
`v1.18.0-meshnomad.3` on, every `.bin` in a release and in a development build carries a
signed build provenance attestation. Check a downloaded file with the
[GitHub CLI](https://cli.github.com/):

```
gh attestation verify <file>.bin -R MeshNomad/meshnomad-firmware
```

The check passes only for a file built from this repository by its own workflow.
