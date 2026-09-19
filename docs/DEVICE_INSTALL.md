# Connected Cardputer installation record

Date: 2026-09-19

The initial SesameRemote v0.1.0 build was installed on the connected M5Stack
Cardputer ADV with MAC `70:af:09:df:a9:b8`.

## Verified target

- Physical flash: ESP32-S3, 8 MB
- Partition table before installation, SHA-256:
  `ae1dc715e9c26456fa6904c439a2a4d948c4cc6f5a2fe744cb15e0b368fea910`
- Existing final partition: `cardputer-lofi>`, ending at `0x630000`
- Added partition: `SesameRemote`, OTA subtype 4
- Offset: `0x630000`
- Size: `0x140000` (1,310,720 bytes)
- End: `0x770000`, leaving `0x90000` bytes unallocated

BrokenSignal was already absent from the freshly read table. The new entry
uses only the free region after LoFi. Hermes, AgentConsole, ORACLE, LoFi, the
factory launcher, `apps_nvs`, and `apps_ota` were not included in the
application erase/write range.

The updated partition-table image has SHA-256
`d16b9c0bf40943714c0846d3dbe95621c38e135b459902b2dca8d52146b417b4`.
It was generated and round-trip validated with Espressif's partition-table
tool, written at `0x8000`, and verified against flash.

## Recovery copies

The pre-install table and OTA metadata are stored under `backups/` (which is
deliberately Git-ignored):

- `Cardputer-ADV-70AF09DFA9B8-partition-table-before-SesameRemote.bin`
  — SHA-256
  `ae1dc715e9c26456fa6904c439a2a4d948c4cc6f5a2fe744cb15e0b368fea910`
- `Cardputer-ADV-70AF09DFA9B8-apps-ota-before-SesameRemote.bin`
  — SHA-256
  `2df517a435d6eec390ddc3561107e15ca563087d1de5cd52a0f3d7cbdbe37517`
- `Cardputer-ADV-70AF09DFA9B8-partition-table-with-SesameRemote.bin`
  — SHA-256
  `d16b9c0bf40943714c0846d3dbe95621c38e135b459902b2dca8d52146b417b4`

The older complete BrokenSignal-slot backup is retained separately for
historical recovery.

## Previously installed image

- Descriptor name: `SesameRemote`
- Version: `0.1.0`
- Image size: 975,936 bytes
- Image SHA-256:
  `efddd3d25482b13d2d116cd69dfa7b2a2ce8322d1da6e60ff59758bd2ea8bae9`
- Remaining partition capacity: 334,784 bytes
- `esptool verify_flash`: successful, digest matched

The M5Apps serial log recognized `SesameRemote` as an app installed by the
user and displayed its one-time management notice. Press Enter to dismiss
that notice, then select `SesameRemote` and press Enter to launch it. The byte
verification and launcher recognition do not certify live Wi-Fi/robot
movement; perform the supported-off-floor check in the installation guide on
first launch.

This record intentionally preserves the exact hash that was verified on the
device. Later repository builds must be flashed and verified separately before
their hashes are added here.
