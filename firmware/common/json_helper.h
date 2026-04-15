/**
 * json_helper.h
 * Helper functions for JSON building/parsing in ESPHome lambdas
 * Used by PlantHub firmware for MQTT message construction
 */

#pragma once

#include <string>
#include <map>
#include "mdns.h"
namespace planthub {

/**
 * Build a JSON object string from a map of sensor values
 * Output: "soilMoisture":42.7,"temperature":23.4
 */
inline std::string buildSensorJson(const std::map<std::string, float>& values) {
    std::string json = "";
    bool first = true;

    for (const auto& pair : values) {
        if (!first) {
            json += ",";
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", pair.second);
        json += "\"" + pair.first + "\":" + std::string(buf);
        first = false;
    }

    return json;
}

/**
 * Build a JSON object string from a map of actuator states
 * Output: "waterPump":"ON","ventFan":"OFF"
 */
inline std::string buildActuatorJson(const std::map<std::string, std::string>& states) {
    std::string json = "";
    bool first = true;

    for (const auto& pair : states) {
        if (!first) {
            json += ",";
        }
        json += "\"" + pair.first + "\":\"" + pair.second + "\"";
        first = false;
    }

    return json;
}

/**
 * Escape a string for safe JSON inclusion
 */
inline std::string escapeJson(const std::string& input) {
    std::string output;
    output.reserve(input.size() + 10);

    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }

    return output;
}

/**
 * Simple JSON string value extractor
 * Finds "key":"value" and returns value
 * Returns defaultValue if key not found
 */
inline std::string extractJsonString(const std::string& json, const std::string& key, const std::string& defaultValue = "") {
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    size_t start = pos + searchKey.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return defaultValue;

    return json.substr(start, end - start);
}

/**
 * Simple JSON integer value extractor
 * Finds "key":123 and returns integer value
 * Returns defaultValue if key not found
 */
inline int extractJsonInt(const std::string& json, const std::string& key, int defaultValue = 0) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    size_t start = pos + searchKey.length();
    // Skip whitespace
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    size_t end = json.find_first_of(",}] \t\n\r", start);
    if (end == std::string::npos) end = json.size();

    std::string numStr = json.substr(start, end - start);
    int result = defaultValue;
    int temp = 0;
    bool valid = true;
    for (char c : numStr) {
        if (c >= '0' && c <= '9') {
            temp = temp * 10 + (c - '0');
        } else if (c == '-' && temp == 0) {
            // Negative sign - handle in next pass
        } else {
            valid = false;
            break;
        }
    }
    if (valid && !numStr.empty() && numStr[0] == '-') {
        result = -temp;
    } else if (valid) {
        result = temp;
    }
    return result;
}

} // namespace planthub
