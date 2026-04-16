#!/usr/bin/env bash
# ============================================================================
# PlantHub Provisioning Flow — curl walkthrough
# ============================================================================
#
# This script walks through the device provisioning API using curl.
# It's meant to be read and run step-by-step, not executed as a whole.
#
# Prerequisites:
#   - curl and jq installed
#   - A PlantHub backend running (https://planthub.online or local)
#   - A PlantHub user account (for the claim step)
#
# The device provisioning key below matches the alpha backend.
# ============================================================================

set -euo pipefail

BACKEND_URL="https://planthub.online"
DEVICE_KEY="mfrGhM2Agu5JIALJPITG4oHW2RYbg6FtXKOHscbF3CQ="

# ============================================================================
# Step 1: Self-Register
# ============================================================================
# The ESP32 calls this on first boot after connecting to WiFi.
# Replace the MAC address with your device's actual MAC.

echo "=== Step 1: Self-Register ==="

REGISTER_RESPONSE=$(curl -s -X POST "${BACKEND_URL}/api/v1/devices/self-register" \
  -H "Content-Type: application/json" \
  -H "X-Device-Key: ${DEVICE_KEY}" \
  -d '{
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "firmwareVersion": "1.1.0",
    "hardwareVersion": "esp32c3-zero-v1"
  }')

echo "$REGISTER_RESPONSE" | jq .

# Extract values for subsequent steps
NODE_ID=$(echo "$REGISTER_RESPONSE" | jq -r '.data.nodeId')
CLAIM_CODE=$(echo "$REGISTER_RESPONSE" | jq -r '.data.claimCode')
PROV_TOKEN=$(echo "$REGISTER_RESPONSE" | jq -r '.data.provisioningToken')

echo ""
echo "Node ID:      ${NODE_ID}"
echo "Claim Code:   ${CLAIM_CODE}"
echo "Prov Token:   ${PROV_TOKEN:0:16}..."
echo ""

# ============================================================================
# Step 2: Poll MQTT Config (before claiming — returns UNCLAIMED)
# ============================================================================
# The device polls this every 10 seconds until the user claims it.

echo "=== Step 2: Poll MQTT Config (unclaimed) ==="

curl -s -X GET "${BACKEND_URL}/api/v1/devices/${NODE_ID}/mqtt-config?token=${PROV_TOKEN}&mac=AA:BB:CC:DD:EE:FF" \
  -H "X-Device-Key: ${DEVICE_KEY}" | jq .

echo ""

# ============================================================================
# Step 3: Claim Device (requires user JWT)
# ============================================================================
# This step is done by the user in the PlantHub dashboard.
# To test via curl, you need a valid JWT token from /api/v1/auth/login.
#
# Uncomment and fill in your JWT to test:

# echo "=== Step 3: Claim Device ==="
#
# JWT_TOKEN="your-jwt-token-here"
#
# curl -s -X POST "${BACKEND_URL}/api/v1/devices/claim" \
#   -H "Content-Type: application/json" \
#   -H "Authorization: Bearer ${JWT_TOKEN}" \
#   -d "{\"claimCode\": \"${CLAIM_CODE}\"}" | jq .

# ============================================================================
# Step 4: Poll MQTT Config (after claiming — returns READY)
# ============================================================================
# After the user claims the device, this returns full MQTT configuration.
#
# Uncomment after claiming:

# echo "=== Step 4: Poll MQTT Config (claimed) ==="
#
# curl -s -X GET "${BACKEND_URL}/api/v1/devices/${NODE_ID}/mqtt-config?token=${PROV_TOKEN}&mac=AA:BB:CC:DD:EE:FF" \
#   -H "X-Device-Key: ${DEVICE_KEY}" | jq .

echo "=== Done ==="
echo ""
echo "Next steps:"
echo "  1. Log in to ${BACKEND_URL}"
echo "  2. Go to Devices > Claim Device"
echo "  3. Enter claim code: ${CLAIM_CODE}"
echo "  4. The device will pick up MQTT config on next poll"
