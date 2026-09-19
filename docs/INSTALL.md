# Installation

The release BIN is an **application-only ESP32-S3 image** for M5Apps.

## Recommended: M5Apps Installer and SD

1. Build or unpack the release package.
2. Copy `cardputer-sesame-robot-<version>.bin` to the Cardputer SD card.
3. Start **M5Apps → Installer → SD**.
4. Select the BIN and choose the application slot deliberately.
5. Reboot into Sesame Remote.

Use the Cardputer's side **Home** button to return to the M5Apps launcher.

M5Apps layouts have fixed application slots. If every slot is occupied, the
installer must replace one existing application. Back up that application's
BIN or keep its release package before replacing it.

## Safety

- Never write this BIN at `0x000000`; that would overwrite boot data.
- Never replace the partition table, launcher, `apps_nvs`, or `apps_ota` as
  part of an ordinary application install.
- For direct USB installation, first read the physical partition table and
  verify the chosen slot's exact offset and size. Do not reuse offsets from
  another Cardputer.
- Keep the robot supported off the floor for the first control test.

## First connection

On boot the remote tries `Sesame-Controller` with password `12345678`. If it
is unavailable, the network list opens automatically. Press `N` later to scan
again. On another network, configure the robot onto the same network first so
`sesame-robot.local` can be found.
