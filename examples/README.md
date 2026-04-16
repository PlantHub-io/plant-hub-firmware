# Examples

Reference payloads and scripts for the PlantHub firmware provisioning flow. Use these to understand the device-to-backend communication protocol or to test the API with curl.

## For alpha testers

You don't need anything in this folder to get a device online. Follow the [main README](../README.md) instead. These examples are here if you're curious about what's happening under the hood.

## For developers

### API Payloads (`api-payloads/`)

JSON request/response examples for the backend provisioning endpoints:

| File | Endpoint | Description |
|------|----------|-------------|
| `self-register-request.json` | `POST /api/v1/devices/self-register` | Device sends MAC + firmware info |
| `self-register-response.json` | (response) | Backend returns nodeId + claim code + token |
| `mqtt-config-unclaimed.json` | `GET /api/v1/devices/{nodeId}/mqtt-config` | Response while waiting for user to claim |
| `mqtt-config-ready-mosquitto.json` | (response) | MQTT config after claim (Mosquitto broker) |
| `mqtt-config-ready-aws.json` | (response) | MQTT config after claim (AWS IoT Core) |
| `claim-device-request.json` | `POST /api/v1/devices/claim` | User claims device with 8-char code |
| `claim-device-response.json` | (response) | Claimed device details |

### MQTT Messages (`mqtt-messages/`)

JSON payloads published/received on MQTT topics:

| File | Topic | Direction |
|------|-------|-----------|
| `sensor-telemetry.json` | `{tenantId}/{nodeId}/sensors` | Device -> Backend |
| `capabilities.json` | `{tenantId}/{nodeId}/capabilities` | Device -> Backend (on boot) |
| `actuator-command.json` | `{tenantId}/{nodeId}/actuators` | Backend -> Device |
| `actuator-state.json` | `{tenantId}/{nodeId}/actuator-state` | Device -> Backend |
| `system-command-reset.json` | `{tenantId}/{nodeId}/system` | Backend -> Device |

### curl Scripts (`curl/`)

| File | Description |
|------|-------------|
| `provisioning-flow.sh` | Walk through the full provisioning flow using curl |

## Provisioning flow overview

```
ESP32 boots (no saved config)
    |
    v
Raises WiFi AP "PlantHub-XXXX"
    |
    v
User connects phone, enters WiFi creds via captive portal
    |
    v
POST /api/v1/devices/self-register   (X-Device-Key header)
    |  -> Returns: nodeId, claimCode, provisioningToken
    v
Device displays claim code on web UI
    |
    v
User enters claim code in PlantHub dashboard
    |  POST /api/v1/devices/claim     (JWT auth)
    v
GET /api/v1/devices/{nodeId}/mqtt-config  (polling every 10s)
    |  -> Returns: MQTT broker details + TLS certs (if AWS)
    v
Device saves config to NVS flash, reboots
    |
    v
Connects to MQTT, publishes capabilities + sensor telemetry
```
