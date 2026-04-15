# Customization Guide

Everything you might need to change to adapt the firmware to your hardware. Order is easiest → most involved.

---

## My pins are different

Edit the `substitutions:` block at the top of your device YAML (e.g. `firmware/devices/my-device.yaml`). Every `${...}` placeholder used deeper in the file resolves against this block, so changing a pin here is enough.

```yaml
substitutions:
  pump_pin: GPIO10    # was GPIO3
  i2c_sda_pin: GPIO18
  i2c_scl_pin: GPIO19
```

If you wire a sensor type the repo already supports but on unusual pins, that's all you need. Re-run `esphome run ...` to flash.

---

## My board is different

Change the `esp32.board` key. Look up the PlatformIO identifier at [registry.platformio.org/platforms/platformio/espressif32/boards](https://registry.platformio.org/platforms/platformio/espressif32/boards). Examples:

```yaml
esp32:
  board: esp32dev           # classic 38-pin DevKit
  # board: lolin_s2_mini    # ESP32-S2 Mini
  # board: lilygo-t-display # T-Display S3
```

Don't change the `framework.type: esp-idf` line. The custom MQTT component depends on ESP-IDF primitives (runtime TLS cert loading from NVS) that the Arduino framework can't do.

---

## Soil-moisture calibration

Capacitive probes vary significantly between manufacturers. The defaults in the examples are starting points, not gospel.

1. Flash the device with default `soil_dry_value: "2.8"` / `soil_wet_value: "1.2"`.
2. Watch the raw ADC sensor in the on-device web UI at `http://<device-ip>/` or in the serial log (look for `soil_raw`).
3. With the probe in **dry air**, read the raw voltage. That's your `soil_dry_value`.
4. Submerge the probe in **plain water up to (but not past) the line printed on the PCB**. Read the raw voltage. That's your `soil_wet_value`.
5. Update the substitutions and re-flash.

Typical values: 2.6 – 3.0 V dry, 1.0 – 1.4 V wet. If your wet value is higher than dry, swap them — your probe is wired the other way around.

---

## Adding a new sensor

Goal: get a reading from hardware the repo doesn't already support, and have it show up on your PlantHub dashboard.

### 1. Find an ESPHome platform

Nearly every common sensor has a built-in ESPHome component. Browse [esphome.io/components/sensor](https://esphome.io/components/sensor/). Popular ones:

| Sensor               | Platform            |
|----------------------|---------------------|
| DHT11 / DHT22 / AM2302 | `dht`             |
| SHT3x / SHT4x        | `sht3xd` / `sht4x`  |
| DS18B20 (1-Wire)     | `dallas`            |
| SCD30 / SCD4x CO₂    | `scd30` / `scd4x`   |
| TSL2561 / TSL2591    | `tsl2561` / `tsl2591` |
| SEN0244 pH probe     | `ph_sensor` (community) |

### 2. Create the sensor package

Copy the template:

```bash
cp firmware/sensors/_template.yaml firmware/sensors/dht22.yaml
```

Replace the `sensor:` block with the platform's ESPHome config, and write readings into `sensor_values` via `on_value`:

```yaml
# firmware/sensors/dht22.yaml
sensor:
  - platform: dht
    pin: ${dht_pin}
    model: DHT22
    update_interval: ${sensor_update_interval}
    temperature:
      name: "Air Temperature"
      id: air_temp
      on_value:
        then:
          - lambda: 'id(sensor_values)["airTemperature"] = x;'
    humidity:
      name: "Air Humidity"
      id: air_humidity
      on_value:
        then:
          - lambda: 'id(sensor_values)["airHumidity"] = x;'
```

`bme280.yaml` and `soil_moisture_adc.yaml` in the same folder are good references.

### 3. Declare the pin in your device YAML

```yaml
substitutions:
  dht_pin: GPIO5
```

### 4. Advertise capabilities

In your device YAML, extend `capabilities_sensors`. The JSON shape is:

```json
{
  "sensorKey": "airTemperature",
  "sensorType": "dht22",
  "unit": "°C",
  "displayName": "Air Temperature",
  "minValue": -40,
  "maxValue": 80,
  "precision": 0.1
}
```

Inline as a `msg +=` line (mind the trailing commas — every line needs one except the last):

```yaml
capabilities_sensors: |-
  msg += "{\"sensorKey\":\"airTemperature\",\"sensorType\":\"dht22\",\"unit\":\"°C\",\"displayName\":\"Air Temperature\",\"minValue\":-40,\"maxValue\":80,\"precision\":0.1},";
  msg += "{\"sensorKey\":\"airHumidity\",\"sensorType\":\"dht22\",\"unit\":\"%\",\"displayName\":\"Air Humidity\",\"minValue\":0,\"maxValue\":100,\"precision\":0.1}";
```

### 5. Include the package

```yaml
packages:
  # ...
  dht22: !include ../sensors/dht22.yaml
```

### 6. Re-flash

```bash
esphome run firmware/devices/my-device.yaml
```

Within one telemetry interval (default 60 s) the new keys stream to the dashboard. The backend auto-registers them on first sight, so no server-side config is needed. The capabilities block is only used to supply friendly display names and ranges for the dashboard UI.

> **Key naming convention.** Use camelCase for `sensorKey` (`airTemperature`, not `air_temp` or `AirTemperature`). The backend groups history across devices by key — using `soilMoisture` on device A and `soil_moisture` on device B will create two separate series.

---

## Adding a new actuator

Mostly the same workflow as a sensor, plus one extra step: wiring the command router.

### 1. Create the actuator package

```bash
cp firmware/actuators/_template.yaml firmware/actuators/humidifier.yaml
```

Edit the `switch:` block — set the right pin substitution, id, `actuator_states` key, and MQTT actuator-state payload. Model on `vent_fan.yaml` (simplest) or `water_pump.yaml` (with safety auto-off) as fits.

### 2. Declare the pin

```yaml
substitutions:
  humidifier_pin: GPIO5
```

### 3. Advertise capabilities

```yaml
capabilities_actuators: |-
  msg += "{\"actuatorKey\":\"humidifier\",\"actuatorType\":\"relay\",\"displayName\":\"Humidifier\",\"supportedCommands\":[\"ON\",\"OFF\"],\"controlType\":\"BINARY\",\"durationSupported\":false}";
```

If the actuator accepts a duration (pump, mist sprayer), set `"durationSupported":true` and `"maxDurationSeconds":<limit>`.

### 4. Route commands

Add a branch in `command_router`:

```yaml
command_router: |-
  if (actuator == "waterPump") {
    // ... existing branch ...
  } else if (actuator == "humidifier") {
    if (action == "ON")  id(humidifier).turn_on();
    if (action == "OFF") id(humidifier).turn_off();
  } else {
    ESP_LOGW("command", "Unknown actuator: %s", actuator.c_str());
  }
```

### 5. Include and re-flash

```yaml
packages:
  humidifier: !include ../actuators/humidifier.yaml
```

```bash
esphome run firmware/devices/my-device.yaml
```

The dashboard will show a new control for your actuator within ~30 s of boot once the capabilities message is processed.

---

## Changing telemetry cadence

```yaml
substitutions:
  sensor_update_interval: "10s"   # how often a sensor is sampled
  telemetry_interval: "30s"       # how often a batch is pushed to MQTT
```

Don't go below ~5 s telemetry unless you're debugging — it burns battery and generates dashboard noise.

---

## When ESPHome YAML merge bites you

If you see a symptom where "everything compiles and flashes but my new sensor/actuator never appears on the dashboard," you might have collided with ESPHome's package merge rules. The rule is:

> Any top-level key that appears in multiple packages is **overwritten**, not merged. Lists like `sensor:` and `switch:` are appended. Dicts like `esphome: on_boot:` are overwritten.

The one practical gotcha: **never add a second `esphome:` block** in a device YAML or in a custom package. All `on_boot` entries live in `common/base.yaml` for this reason. If you need to run code at boot, wire it in via a `script:` + `interval:` instead of a new `on_boot:`.
