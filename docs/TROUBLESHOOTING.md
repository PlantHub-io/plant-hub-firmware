# Troubleshooting

Symptoms in **bold**, diagnosis and fix underneath. Work through top to bottom — earlier entries cover the most common first-time issues.

---

### **`esphome` command not found / `pip install` fails**

Make sure you installed with the same Python that's on your PATH:

```bash
python --version      # should be 3.9+
python -m pip install esphome==2026.4.0
```

On Windows, if `esphome` still isn't found, use `python -m esphome run ...` instead.

---

### **`esphome run` fails with "Failed to connect: Could not enter bootloader"**

1. You're missing a USB-to-serial driver. Most ESP32-C3/S3 boards use CP210x or CH340. Install the driver:
   - CP2102: [silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
   - CH340: [wch-ic.com/downloads/CH341SER_EXE.html](https://www.wch-ic.com/downloads/CH341SER_EXE.html)
2. Your USB cable might be charge-only. Swap it for a known-good data cable.
3. Hold the BOOT button (if your board has one) while plugging in, then run `esphome run ...`.

On Windows, you can also pass `--device COMx` if ESPHome picks the wrong port.

---

### **First compile takes forever**

Expected on first run — ESPHome downloads the ESP-IDF toolchain (~1 GB). Subsequent builds are cached under `.esphome/` and take under a minute.

If the download stalls, try:

```bash
esphome clean firmware/devices/my-device.yaml
esphome run firmware/devices/my-device.yaml
```

---

### **Device boots but my phone never sees a `PlantHub-XXXX` Wi-Fi**

1. Check the serial log (`esphome logs firmware/devices/my-device.yaml`) for `AP fallback enabled`. If it never appears, the device is still trying to connect to saved credentials. Factory-reset: hold the reset button for 5 s, or erase flash:

   ```bash
   esphome clean firmware/devices/my-device.yaml
   esptool.py --chip esp32c3 erase_flash      # adjust --chip as needed
   ```

2. On iOS 17+ and Android 14+, the Wi-Fi list sometimes hides "open" networks. Look in **Settings → Wi-Fi → Other Networks**, or manually type the SSID if you see it in the serial log.

3. The AP is 2.4 GHz only. That's fine for joining, but your phone might default to 5 GHz — turn off 5 GHz for a minute if your Wi-Fi chip prefers it.

---

### **Captive portal doesn't open automatically**

Android: captive portal detection usually triggers within 10 s. If nothing happens, open any HTTP (not HTTPS) site — e.g. `http://example.com` — and the redirect should kick in.

iOS: pull down from the top-right to open Control Center; tap the Wi-Fi block; the captive portal login sheet should appear. If not, open Safari and go to `http://192.168.4.1`.

---

### **Captive portal loads but submitting Wi-Fi credentials does nothing**

Check the serial log after you tap Save. Common causes:

- Wrong SSID / password. The device logs `wifi: Connect attempt failed, reason=XX`. Reasons: 15 = auth fail, 201 = SSID not found.
- 5 GHz-only network. ESP32 hardware does not support 5 GHz. Use the 2.4 GHz band of your router.
- Hidden SSID. The stock captive portal doesn't support hidden networks — unhide temporarily or put the SSID into `firmware/secrets.yaml` and pre-provision.

---

### **No claim code appears in the captive portal or logs**

You've joined Wi-Fi but self-registration is failing. Look for `self_register` lines in the serial log.

| Log                                  | Meaning / fix                                                                 |
|--------------------------------------|-------------------------------------------------------------------------------|
| `HTTP request failed`                | Device can't reach `planthub.online`. Check your router allows outbound 443.  |
| `HTTP 401` / `HTTP 403`              | `device_provisioning_key` is wrong. Re-check `firmware/secrets.yaml`.         |
| `HTTP 429`                           | Rate-limited. Wait 30 s and reboot.                                           |
| `Time not synced`                    | SNTP is slow. Wait another minute — TLS can't validate without a valid clock. |

---

### **I entered the claim code but the dashboard still shows the device as unclaimed**

The claim is written to the backend instantly, but the firmware polls for its MQTT config every 10 s. Wait up to 30 s. If still nothing:

1. On the dashboard, delete the device and re-enter the claim code.
2. Make sure you entered the code displayed in **this** boot's captive portal. If you rebooted the device, a new code was issued.

---

### **Device claims successfully but disconnects from MQTT in a loop**

Usually TLS-related:

- **Clock wrong.** Check `esphome logs` for "Time synchronized via NTP". If it never appears, confirm outbound UDP 123 is allowed.
- **Outbound 8883 blocked.** Some corporate and captive networks block MQTT over TLS. Try on a phone hotspot to confirm.
- **Cert truncated.** If your NVS partition is too small to hold the cert, writes fail silently. The repo ships `partitions.csv` with a 20 KB NVS — don't override it.

Serial log snippet indicating success: `mqtt: Connected to MQTT broker`.

---

### **The dashboard shows the device online but no sensor readings arrive**

1. Check the serial log for `telemetry: Published: {...}` every `telemetry_interval`. If absent, the telemetry script isn't running.
2. Check `sensor_values` is being populated. Each sensor YAML should have an `on_value: -> lambda: 'id(sensor_values)[...] = x;'`.
3. Confirm the sensor has a physical reading. The on-device web UI at `http://<device-ip>/` shows live values — if they're `NaN`, the sensor isn't reading (wrong pin, wrong I2C address, broken wiring).

---

### **My new sensor shows up as `unknownSensorKey` on the dashboard**

You forgot to add a line to `capabilities_sensors` in your device YAML. The backend still accepts the reading and stores it, but without the capability metadata it has no display name, unit, or range. Add the line, re-flash, reboot, and it should replace the unknown entry within a minute.

---

### **`esphome` says "fatal: unable to locate a `secrets.yaml`"**

You skipped step 2 of the README:

```bash
cp firmware/secrets.yaml.example firmware/secrets.yaml
```

---

### **I bricked the board / want to start from scratch**

```bash
esptool.py --chip esp32c3 erase_flash     # or esp32s3, esp32, etc.
esphome run firmware/devices/my-device.yaml
```

Erase wipes everything including the NVS-stored claim. You'll need to re-claim the device on the dashboard with a fresh code.

---

## Still stuck?

Open a new issue with:

- Board type and what sensors/actuators you have connected
- The last ~50 lines of `esphome logs` output
- The contents of your device YAML (redact `secrets.yaml`)

The first two answer 90 % of questions without a back-and-forth.
