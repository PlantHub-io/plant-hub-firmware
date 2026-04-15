# Hardware Guide

This doc covers the two reference boards that ship with working configs, the sensors and actuators included in the repo, and the conventions you should follow when bringing your own hardware.

---

## Reference boards

### ESP32-C3 Zero (and compatible SuperMini boards)

| GPIO   | Role                          |
|--------|-------------------------------|
| GPIO0  | Soil moisture sensor (ADC)    |
| GPIO3  | Water pump relay              |
| GPIO4  | Ventilation fan relay         |
| GPIO6  | I2C SDA (BME280 + BH1750)     |
| GPIO7  | I2C SCL (BME280 + BH1750)     |
| GPIO8  | Status LED                    |
| GPIO9  | Factory-reset button → GND    |

Board string: `esp32-c3-devkitm-1`. Config file: `firmware/devices/example-c3zero.yaml`.

### ESP32-S3 Zero

| GPIO   | Role                          |
|--------|-------------------------------|
| GPIO1  | Soil moisture sensor (ADC)    |
| GPIO3  | Water pump relay              |
| GPIO4  | Ventilation fan relay         |
| GPIO8  | I2C SDA                       |
| GPIO9  | I2C SCL                       |
| GPIO48 | Status LED (on-board RGB)     |
| GPIO2  | Factory-reset button → GND    |

Board string: `esp32-s3-devkitc-1`. Config file: `firmware/devices/example-s3zero.yaml`.

### Other ESP32 boards

Any ESP32 with Wi-Fi works. Start from `firmware/devices/my-device.yaml.example` and set the `board:` key to your board's PlatformIO identifier. Useful lookups:

- [PlatformIO board list](https://registry.platformio.org/platforms/platformio/espressif32/boards)
- [ESPHome + ESP-IDF framework docs](https://esphome.io/components/esp32.html)

---

## Sensors included

### BME280 — temperature, humidity, pressure (I2C)

File: `firmware/sensors/bme280.yaml`

Wiring (reference C3 Zero pins):

```
BME280            ESP32-C3 Zero
------            -------------
VCC     ────────→ 3.3V
GND     ────────→ GND
SDA     ────────→ GPIO6
SCL     ────────→ GPIO7
```

I2C address: `0x76` default, some breakouts are `0x77`. Adjust `bme280_address` in your device YAML if needed.

### BH1750 — ambient light (I2C)

File: `firmware/sensors/bh1750.yaml`

Shares the I2C bus with the BME280 — same SDA/SCL pins, separate address (`0x23`).

### Capacitive soil moisture probe (analog)

File: `firmware/sensors/soil_moisture_adc.yaml`

Wiring:

```
Soil probe        ESP32-C3 Zero
----------        -------------
VCC      ───────→ 3.3V
GND      ───────→ GND
AOUT     ───────→ GPIO0  (ADC)
```

You must calibrate `soil_dry_value` / `soil_wet_value` for your specific probe. See `docs/CUSTOMIZATION.md`.

---

## Actuators included

All actuators in this repo are GPIO switches driving relay modules or small MOSFET boards. The generic wiring is:

```
ESP32 GPIO  ─→ Relay IN
5V          ─→ Relay VCC
GND         ─→ Relay GND (common with ESP32 GND)

Relay NO/NC ─→ Load + / Load −
```

Most common "optocoupled 3.3 V-tolerant" relay modules are active-LOW. If your pump stays on when it should be off, flip `inverted: true` under the GPIO pin config, or wire into the NC (normally-closed) contact.

| File                                | Purpose                               | Safety feature                        |
|-------------------------------------|---------------------------------------|---------------------------------------|
| `firmware/actuators/water_pump.yaml`| Pump/valve                            | Auto-off after `${pump_max_duration}` |
| `firmware/actuators/vent_fan.yaml`  | Fan/exhaust                           | —                                     |
| `firmware/actuators/grow_light.yaml`| Light (on/off; PWM-capable platform)  | —                                     |
| `firmware/actuators/heater.yaml`    | Heating element                       | —                                     |
| `firmware/actuators/mist_sprayer.yaml` | Misting solenoid                   | —                                     |
| `firmware/actuators/shade_blind.yaml`  | Shade motor (PWM 0-100 %)          | —                                     |

If you add a high-current load (pump, heater), always add a safety auto-off in the actuator YAML. See `water_pump.yaml` for the pattern.

---

## Power supply notes

These are the most common first-time mistakes:

- **Common ground.** If you use a separate 5 V brick for the relay module, its GND must be tied to the ESP32 GND. Otherwise the relay will trigger unpredictably.
- **3.3 V for sensors, 5 V for relays.** BME280, BH1750, and most capacitive soil probes are 3.3 V devices. Feeding them 5 V works on many boards but can skew ADC readings or damage the sensor.
- **USB current.** ESP32 + one relay clicking on and off is fine on USB. A real pump or heater needs its own PSU — don't try to power it from USB through the relay.
- **Deep sleep and ADC.** If you add deep-sleep later, remember that the soil-moisture ADC attenuation applies only while the ADC is powered. Readings stabilize a few hundred ms after wake-up.

---

## Wiring gotchas by board

- **ESP32-C3 SuperMini clones** occasionally route GPIO9 to a boot button — holding it during power-on puts the board in download mode. The factory-reset logic detects a sustained 5-second hold so a quick accidental press won't nuke your NVS.
- **ESP32-S3 Zero** has no physical button on some variants. Either solder a tactile switch to your mapped `factory_reset_pin`, or skip the reset path (the device is perfectly usable without it; you can always re-flash).
- **Classic ESP32 DevKit v1** — GPIO34-39 are input-only. Don't map `pump_pin`, `fan_pin`, or any switch to those pins.
