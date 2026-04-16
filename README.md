# PlantHub Firmware

ESP32 firmware for alpha testers of [PlantHub](https://planthub.online) — the multi-tenant IoT platform for smart plant care. Flash this firmware to an ESP32, wire up your sensors, connect it to Wi-Fi from your phone, and your device shows up in your PlantHub account.

**Status:** Alpha. The onboarding flow is stable, but expect bugs. Please report anything weird at the [issue tracker](../../issues).

---

## Get your device online in 10 minutes

**What you need:**

- An ESP32 board (C3, S3, or classic ESP32 — any variant with Wi-Fi)
- At least one sensor (BME280 and capacitive soil probes are cheapest) and/or one relay for an actuator
- A USB cable that supports data (not charge-only)
- A computer with Python 3.9+
- A PlantHub account — sign up at [planthub.online](https://planthub.online)
- A phone on 2.4 GHz Wi-Fi (ESP32 does not support 5 GHz)

### 1. Install ESPHome (pinned stable)

```bash
pip install esphome==2026.4.0
```

Newer versions may work but are not tested. If `pip` isn't available, see [esphome.io/install](https://esphome.io/install/).

### 2. Clone

```bash
git clone https://github.com/vic7z/plant-hub-firmware.git
cd plant-hub-firmware
```

The repo ships with `firmware/devices/secrets.yaml` already configured for the alpha backend at planthub.online — no manual setup needed.

### 3. Pick a device config

Three starting points with real sensors, from easiest to most flexible:

| File | Use when |
|---|---|
| `firmware/devices/my-device.yaml.example` | Starting minimal — one sensor, one pump. Edit 3-4 lines, flash. |
| `firmware/devices/example-c3zero.yaml` | ESP32-C3 Zero wired per [docs/HARDWARE.md](docs/HARDWARE.md). Works as-is. |
| `firmware/devices/example-s3zero.yaml` | ESP32-S3 Zero wired per [docs/HARDWARE.md](docs/HARDWARE.md). Works as-is. |

**No sensors?** Use a mock device config instead — see [Mock devices](#mock-devices-no-sensors-required) below.

For a custom board or sensor mix, copy `my-device.yaml.example` and see [docs/CUSTOMIZATION.md](docs/CUSTOMIZATION.md).

```bash
cp firmware/devices/my-device.yaml.example firmware/devices/my-device.yaml
# Edit my-device.yaml — at minimum set `device_id` and the pin substitutions
```

### 4. Flash

Plug in the ESP32 over USB, then:

```bash
esphome run firmware/devices/my-device.yaml
```

This compiles, uploads over USB, and tails the serial logs. First compile takes a few minutes while ESP-IDF toolchain is fetched.

### 5. Onboard

1. On first boot the device has no Wi-Fi saved, so it raises an access point named `PlantHub-XXXX` (password `planthub-setup`). Connect your phone to it.
2. A captive portal opens automatically. Enter your home Wi-Fi SSID and password.
3. The device joins your network, registers itself with the backend, and shows an **8-character claim code** in the captive portal (and in the serial log).
4. Log in to [planthub.online](https://planthub.online) → **Devices** → **Claim Device** → paste the code.
5. The device picks up its MQTT credentials, reboots, and starts streaming data within ~30 seconds.

Power-cycle behaviour: once claimed, the device reads its saved credentials from NVS flash and reconnects on every boot. No re-onboarding needed.

Stuck? → [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

---

## Mock devices (no sensors required)

Don't have sensors wired up yet? Flash a mock device config to simulate a live node. These generate realistic random sensor data every 30 seconds, so you can test the full pipeline — provisioning, dashboard, AI agent, actuator commands — with just a bare ESP32 board.

| File | Theme | Mock sensors | Actuators |
|---|---|---|---|
| `firmware/devices/mock-greenhouse.yaml` | Greenhouse Controller (C3) | CO2, water level, leaf wetness, soil EC, soil temp | Heater, shade blind |
| `firmware/devices/mock-weather-station.yaml` | Weather Station (C3) | Wind speed, rainfall, UV index, air temp, ambient light | Grow light, mist sprayer |
| `firmware/devices/mock-hydroponic.yaml` | Hydroponic Garden (S3) | Water pH, water temp, dissolved oxygen, TDS, light PAR | Water pump, vent fan |

```bash
# Example: flash the greenhouse mock on an ESP32-C3 Zero
esphome run firmware/devices/mock-greenhouse.yaml
```

The onboarding flow is identical to real devices. Actuator GPIOs are mapped but work without relays connected — commands are accepted and logged, they just don't toggle anything physical.

---

## Bringing your own sensors and actuators

The backend auto-detects whatever sensor keys the firmware publishes, so there is no schema to update server-side. You only need to:

1. Pick an ESPHome sensor/actuator platform for your hardware ([esphome.io/components/sensor](https://esphome.io/components/sensor/), [esphome.io/components/switch](https://esphome.io/components/switch/)).
2. Copy `firmware/sensors/_template.yaml` (or `firmware/actuators/_template.yaml`) to a new file, paste the platform block, and write readings into the shared `sensor_values` map.
3. Include the new file from your device YAML's `packages:` block and add a line to `capabilities_sensors` / `capabilities_actuators`.

Full walkthrough with examples: [docs/CUSTOMIZATION.md](docs/CUSTOMIZATION.md).

---

## Repository layout

```
plant-hub-firmware/
├── README.md                # You are here
├── LICENSE                  # MIT
├── docs/
│   ├── HARDWARE.md          # Supported boards, pinouts, wiring diagrams
│   ├── CUSTOMIZATION.md     # Adapting pins, adding sensors/actuators, calibration
│   ├── TROUBLESHOOTING.md   # Common problems and fixes
│   └── PROVISIONING.md      # Deep dive into the onboarding flow & API
├── examples/
│   ├── api-payloads/        # Backend API request/response JSON samples
│   ├── mqtt-messages/       # MQTT message format samples
│   └── curl/                # curl scripts to exercise the provisioning flow
└── firmware/
    ├── partitions.csv       # 20 KB NVS partition (needed for TLS certs)
    ├── common/              # Shared — don't modify
    ├── sensors/             # Physical + mock sensor modules
    ├── actuators/           # Actuator modules + _template.yaml
    └── devices/             # Device configs + secrets.yaml
```

---

## Factory reset

Hold the factory-reset button (GPIO9 on C3, GPIO2 on S3, or whatever pin you mapped to `factory_reset_pin`) for 5 seconds. The device erases its NVS config, reboots, and comes back up in fresh-provisioning mode. Use this if you claimed a device to the wrong account or want to hand it over to someone else.

---

## Security note (alpha)

The `device_provisioning_key` in `firmware/devices/secrets.yaml` is a shared alpha key committed to this repo. It's sufficient for a small set of known testers but will be rotated to per-tester keys before general availability. Don't rely on this key being stable.

If you're running your own backend, edit `firmware/devices/secrets.yaml` to point at your server and use your own key.

---

## Getting help

- Issues and bug reports: use the repo's issue tracker.
- Documentation gaps: PRs to the `docs/` folder are welcome.
- Backend questions (dashboard, accounts, billing): support channel on planthub.online.
