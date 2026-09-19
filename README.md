# Sesame Robot Remote for Cardputer

A Wi-Fi controller for the **M5Stack Cardputer ADV** that discovers and
controls a Sesame quadruped robot. It is packaged as an application-only
ESP32-S3 image for installation through M5Apps.

## Project site

The static project site lives in [`docs/`](docs/) and is ready for GitHub
Pages. A workflow in [`.github/workflows/pages.yml`](.github/workflows/pages.yml)
publishes it automatically after a push to `main` once GitHub Pages is set to
use GitHub Actions.

## Experience

- Tries the robot's default access point first:
  `Sesame-Controller` / `12345678`.
- Searches for `sesame-robot.local` through mDNS after every Wi-Fi join.
- Falls back to the AP gateway (`192.168.4.1`) only on the default Sesame
  network, because the current robot firmware advertises mDNS only after it
  has joined a station network.
- Provides an on-device 2.4 GHz network scanner and password entry screen.
- Shows a neon launch animation before sending each selected pose.
- Moves while a WASD or arrow key is held, sends `stop` on release, and gives
  Space / the Go button an immediate stop action.
- Keeps entered Wi-Fi credentials in RAM only. It never initializes or erases
  the shared M5Apps NVS partition.

## Controls

| Key | Action |
| --- | --- |
| `;` / `.` | Previous / next pose |
| `,` / `/` | Jump one page backward / forward |
| Enter | Animate, then run the selected pose |
| `W A S D` or arrow keys | Move forward/left/back/right until released |
| Space or Go | Emergency stop |
| `N` | Scan and join another Wi-Fi network |
| `R` | Rediscover `sesame-robot.local` |
| Backtick | Back/cancel |
| Side Home button | Return to the M5Apps launcher |

The pose menu includes Rest, Stand, Wave, Dance, Cute, Bow, Shake, Shrug,
Point, Swim, Pushup, Crab, Worm, Freaky and Dead.

## Build

Install PlatformIO, then run:

```sh
pio run -e cardputer-adv-sesame
python3 tools/build_release.py
```

The pinned stack matches the local Cardputer ADV references: PlatformIO
`espressif32@7.0.1`, M5Unified `0.2.17`, M5GFX `0.2.22`, and M5Cardputer
`1.1.1`.

The release command creates an allowlisted package under `dist/`. The BIN is
an application image, not a whole-device image. Use **M5Apps → Installer →
SD**; never flash it at address `0x000000`. See [installation](docs/INSTALL.md).

The current build is also installed and byte-verified in a dedicated
`SesameRemote` slot on the connected development Cardputer. The slot was
created in the free flash left after BrokenSignal was deleted; existing apps
were preserved. Exact offsets, checksums, and recovery details are in
[the device installation record](docs/DEVICE_INSTALL.md).

## Robot compatibility

The controller expects the current Sesame firmware endpoints:

- `GET /api/status`
- `POST /api/command` with `{"command":"wave"}`

For LAN mDNS discovery, the robot and Cardputer must be on the same 2.4 GHz
network and the robot must have joined that network. Direct AP control works
without a router through the guarded gateway fallback.
