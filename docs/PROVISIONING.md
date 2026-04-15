# Provisioning — deep dive

You don't need to read this to use the firmware. It exists so curious testers (and future contributors) can understand what happens under the hood when a fresh device meets the network.

---

## State machine

```
             ┌─────────────┐
  power on → │    INIT     │
             └──────┬──────┘
                    │ nvs_init()
                    ▼
             ┌─────────────┐    claim+mqtt_cfg found    ┌─────────────┐
             │ CHECK_NVS   │ ──────────────────────────→ │ PROVISIONED │
             └──────┬──────┘                             └──────┬──────┘
                    │ nothing saved                             │ MQTT
                    ▼                                           │ connect
             ┌─────────────┐                                    ▼
             │ NEEDS_WIFI  │                              ┌─────────────┐
             └──────┬──────┘                              │  STREAMING  │
                    │ captive portal: SSID+pass           └─────────────┘
                    ▼
             ┌─────────────┐
             │ REGISTERING │ ──► POST /api/v1/devices/self-register
             └──────┬──────┘
                    │ 200 OK: nodeId, claimCode, provisioningToken
                    ▼
             ┌───────────────┐
             │ WAITING_CLAIM │ ──► GET  /api/v1/devices/{nodeId}/mqtt-config
             └──────┬────────┘      every 10 s
                    │ status=READY, mqttConfig delivered
                    ▼
             (save to NVS, soft reboot → CHECK_NVS → PROVISIONED)
```

On factory reset: NVS is wiped and the device re-enters `NEEDS_WIFI`.

---

## Backend endpoints

### `POST /api/v1/devices/self-register`

Called once per onboarding. Idempotent on MAC — calling twice returns the same `nodeId` with a fresh `provisioningToken`.

**Headers**

```
Content-Type: application/json
X-Device-Key: <device_provisioning_key from secrets.yaml>
```

**Request body**

```json
{
  "macAddress": "AA:BB:CC:DD:EE:FF",
  "firmwareVersion": "1.1.0",
  "hardwareVersion": "esp32c3-zero-v1",
  "deviceName": "PlantHub C3 Node"
}
```

**Response 200**

```json
{
  "nodeId": "node-3f1c9b8a",
  "claimCode": "4XQ7RZ2P",
  "provisioningToken": "0a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p"
}
```

The claim code is displayed to the user. The provisioning token is the bearer the firmware uses for the next endpoint.

### `GET /api/v1/devices/{nodeId}/mqtt-config?token=…&mac=…`

Polled every 10 s until the user claims.

**Response while unclaimed**

```json
{ "status": "UNCLAIMED", "nodeId": "node-3f1c9b8a", ... }
```

**Response once claimed (AWS IoT Core)**

```json
{
  "status": "READY",
  "provider": "aws-iot-core",
  "tenantId": "...",
  "nodeId": "node-3f1c9b8a",
  "endpoint": "xxxxxxxxxxxxxx.iot.us-east-1.amazonaws.com",
  "port": 8883,
  "certificatePem": "-----BEGIN CERTIFICATE-----\n...",
  "privateKeyPem": "-----BEGIN RSA PRIVATE KEY-----\n...",
  "sensorTopic":       "{tenantId}/{nodeId}/sensors",
  "actuatorTopic":     "{tenantId}/{nodeId}/actuators",
  "statusTopic":       "{tenantId}/{nodeId}/status",
  "capabilitiesTopic": "{tenantId}/{nodeId}/capabilities"
}
```

The cert + key are delivered **once**. After the firmware writes them to NVS the backend stops returning them; subsequent polls return topics only.

---

## NVS keys

| Key               | Written by       | Read by                  |
|-------------------|------------------|--------------------------|
| `node_id`         | self_register    | check_nvs, planthub_mqtt |
| `tenant_id`       | poll_mqtt_config | check_nvs, planthub_mqtt |
| `mqtt_provider`   | poll_mqtt_config | planthub_mqtt            |
| `mqtt_endpoint`   | poll_mqtt_config | planthub_mqtt (AWS)      |
| `mqtt_cert`       | poll_mqtt_config | planthub_mqtt (AWS)      |
| `mqtt_key`        | poll_mqtt_config | planthub_mqtt (AWS)      |
| `mqtt_broker_url` | poll_mqtt_config | planthub_mqtt (Mosquitto)|
| `mqtt_username`   | poll_mqtt_config | planthub_mqtt (Mosquitto)|
| `mqtt_password`   | poll_mqtt_config | planthub_mqtt (Mosquitto)|

Factory reset clears every one of these.

---

## MQTT topics

All topics are tenant- and node-scoped. The backend subscribes with wildcards like `{tenantId}/+/sensors` and enforces tenant isolation at the subscription level — devices from tenant A cannot publish into tenant B's topic tree.

| Direction | Topic                                   | Payload           |
|-----------|-----------------------------------------|-------------------|
| device →  | `{tenantId}/{nodeId}/capabilities`      | capabilities JSON |
| device →  | `{tenantId}/{nodeId}/sensors`           | telemetry JSON    |
| device →  | `{tenantId}/{nodeId}/status`            | `"online"` / `"offline"` (LWT) |
| device →  | `{tenantId}/{nodeId}/actuator-state`    | `{"actuator":"...","state":"ON"\|"OFF"}` |
| ← backend | `{tenantId}/{nodeId}/actuators`         | command JSON      |
| ← backend | `{tenantId}/{nodeId}/system`            | system command    |

### Telemetry payload (`sensors` topic)

```json
{
  "timestamp": 1787012345,
  "sensors": {
    "soilMoisture": 42.7,
    "temperature": 23.4,
    "humidity": 58.1
  },
  "telemetry": {
    "batteryMv": 4090,
    "rssi": -62,
    "uptimeSec": 7231,
    "freeHeap": 142336
  }
}
```

The `telemetry` block is split off by the backend and written to the device's row rather than the sensor-readings hypertable, so it doesn't bloat time-series storage.

### Capabilities payload (`capabilities` topic)

```json
{
  "firmwareVersion": "1.1.0",
  "hardwareVersion": "esp32c3-zero-v1",
  "sensors": [
    { "sensorKey": "soilMoisture", "sensorType": "capacitive_adc", "unit": "%", "displayName": "Soil Moisture", "minValue": 0, "maxValue": 100, "precision": 0.1 }
  ],
  "actuators": [
    { "actuatorKey": "waterPump", "actuatorType": "relay", "displayName": "Water Pump", "supportedCommands": ["ON", "OFF"], "controlType": "BINARY", "durationSupported": true, "maxDurationSeconds": 120 }
  ]
}
```

Published once per boot, after MQTT connects and before the first telemetry push.

### Command payload (backend → device)

```json
{
  "nodeId": "node-3f1c9b8a",
  "commands": [
    { "actuator": "waterPump", "action": "ON", "durationSeconds": 30 }
  ]
}
```

The firmware parses the `commands` array and dispatches each entry through `command_router` in the device YAML.

---

## Safety guarantees

- **Tenant isolation.** Topics are scoped to `{tenantId}/{nodeId}`. Even if a malicious claim-code guess lands, the TLS cert binds to a specific tenant's thing policy on AWS IoT.
- **Hard actuator limits.** The backend validates command `durationSeconds` against the `maxDurationSeconds` the firmware advertised in capabilities. The firmware applies the same clamp on the device side (see `water_pump.yaml`). Commands exceeding the limit are clipped.
- **Idempotent commands.** MQTT is at-least-once. Pump-like actuators use `mode: restart` scripts so a duplicate "30 s pump" command restarts the 30 s timer instead of stacking to 60 s.
- **Clock-validated TLS.** SNTP must succeed before AWS IoT will accept the TLS handshake. Without a valid clock the device loops in the MQTT reconnect state — documented in [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

## Extending the protocol

If you add a new device → backend message, pick a new topic under `{tenantId}/{nodeId}/` rather than overloading an existing one. The backend's MQTT router dispatches by topic suffix; new suffixes are trivially added. Backend-side routing lives in `MqttMessageRouter` (Spring Boot side of the project, not in this repo).
