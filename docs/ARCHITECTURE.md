# Architecture

The firmware is a single bounded UI/network loop for Cardputer ADV:

1. Join the default Sesame AP or a network chosen from the asynchronous scan.
2. Start an mDNS client and query `sesame-robot.local`.
3. Probe `/api/status`; only the default AP may use the gateway fallback.
4. Render the pose menu and poll robot status periodically.
5. On selection, finish the launch animation before posting `/api/command`.

Movement keys send a direction on key-down and `stop` as soon as that same
WASD or Fn-arrow input is released. Space and Go always try to send `stop`
immediately. No wireless controller can guarantee a release packet after a
Wi-Fi failure, so keep the robot supported off the floor for the first test.

The `esp_wifi_init` wrapper disables Wi-Fi NVS writes. Network credentials are
session-only; this is intentional for M5Apps partition compatibility.
