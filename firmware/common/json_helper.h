/**
 * json_helper.h
 * Helper functions for JSON building/parsing in ESPHome lambdas
 * Used by PlantHub firmware for MQTT message construction
 */

#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include "mdns.h"
namespace planthub {

/**
 * One automation rule, parsed from a compact shadow payload.
 * Field names match the backend's ShadowPayloadBuilder:
 *   id  — short numeric id (devices.local_id), stable per device
 *   s   — sensor key (e.g. "soilMoisture")
 *   op  — operator: "<", "<=", ">", ">=", "==", "!="
 *   t   — threshold value
 *   a   — actuator key (e.g. "waterPump")
 *   c   — command verb ("ON" / "OFF")
 *   d   — duration in seconds (0 if not applicable)
 *   cd  — cooldown in seconds (0 if not applicable)
 */
struct RuleRow {
    int id;
    std::string s;
    std::string op;
    float t;
    std::string a;
    std::string c;
    int d;
    int cd;
};

/**
 * Build a JSON object string from a map of sensor values
 * Output: "soilMoisture":42.7,"temperature":23.4
 *
 * Non-finite values (NaN, +/-Infinity) are skipped — emitting them produces
 * literal "nan" tokens that are not valid JSON, which the backend would reject
 * for the entire payload. A failed sensor read should drop one key, not the
 * whole publish cycle.
 */
inline std::string buildSensorJson(const std::map<std::string, float>& values) {
    std::string json = "";
    bool first = true;

    for (const auto& pair : values) {
        if (std::isnan(pair.second) || std::isinf(pair.second)) {
            continue;
        }
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

/**
 * Simple JSON float value extractor.
 * Finds "key":<number> and returns the float value (supports negatives + decimals).
 * Returns defaultValue if key not found or unparseable.
 */
inline float extractJsonFloat(const std::string& json, const std::string& key, float defaultValue = 0.0f) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    size_t start = pos + searchKey.length();
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;

    size_t end = start;
    while (end < json.size()) {
        char c = json[end];
        if ((c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+' || c == 'e' || c == 'E') {
            end++;
        } else {
            break;
        }
    }
    if (end == start) return defaultValue;

    std::string numStr = json.substr(start, end - start);
    char* endptr = nullptr;
    float v = std::strtof(numStr.c_str(), &endptr);
    if (endptr == numStr.c_str()) return defaultValue;
    return v;
}

/**
 * Parse a compact shadow rules array into a vector<RuleRow>.
 *
 * Expected layout (whitespace-insensitive):
 *   "rules":[
 *     {"id":1,"s":"soilMoisture","op":"<","t":30,"a":"waterPump","c":"ON","d":20,"cd":300},
 *     ...
 *   ]
 *
 * Walks the array character-by-character, slicing each {...} object and reading
 * its abbreviated fields with the existing extract helpers. Tolerant of:
 *   - "rules" appearing under state.desired or at the root
 *   - whitespace between tokens
 *   - missing optional fields (d, cd default to 0)
 *
 * Skips malformed entries silently — on-device evaluation must never crash on
 * a bad rule. Returns an empty vector if "rules" is missing or empty.
 */
inline std::vector<RuleRow> parseShadowDeltaRules(const std::string& json) {
    std::vector<RuleRow> out;

    size_t arrStart = json.find("\"rules\"");
    if (arrStart == std::string::npos) return out;

    size_t bracket = json.find('[', arrStart);
    if (bracket == std::string::npos) return out;

    size_t arrEnd = std::string::npos;
    int depth = 0;
    for (size_t i = bracket; i < json.size(); i++) {
        if (json[i] == '[') depth++;
        else if (json[i] == ']') {
            depth--;
            if (depth == 0) { arrEnd = i; break; }
        }
    }
    if (arrEnd == std::string::npos) return out;

    size_t cursor = bracket + 1;
    while (cursor < arrEnd) {
        size_t objStart = json.find('{', cursor);
        if (objStart == std::string::npos || objStart >= arrEnd) break;

        int objDepth = 0;
        size_t objEnd = std::string::npos;
        for (size_t i = objStart; i < arrEnd; i++) {
            if (json[i] == '{') objDepth++;
            else if (json[i] == '}') {
                objDepth--;
                if (objDepth == 0) { objEnd = i; break; }
            }
        }
        if (objEnd == std::string::npos) break;

        std::string obj = json.substr(objStart, objEnd - objStart + 1);

        RuleRow row;
        row.id = extractJsonInt(obj, "id", 0);
        row.s  = extractJsonString(obj, "s");
        row.op = extractJsonString(obj, "op");
        row.t  = extractJsonFloat(obj, "t", 0.0f);
        row.a  = extractJsonString(obj, "a");
        row.c  = extractJsonString(obj, "c");
        row.d  = extractJsonInt(obj, "d", 0);
        row.cd = extractJsonInt(obj, "cd", 0);

        // Skip rows that are missing essential fields — defensive against truncation.
        if (row.id > 0 && !row.s.empty() && !row.op.empty() && !row.a.empty() && !row.c.empty()) {
            out.push_back(row);
        }
        cursor = objEnd + 1;
    }
    return out;
}

/**
 * Compare an observed sensor value against a rule threshold using the
 * abbreviated operator string. Unknown operators return false.
 */
inline bool ruleOperatorMatches(const std::string& op, float value, float threshold) {
    if (op == "<")  return value <  threshold;
    if (op == "<=") return value <= threshold;
    if (op == ">")  return value >  threshold;
    if (op == ">=") return value >= threshold;
    if (op == "==") return value == threshold;
    if (op == "!=") return value != threshold;
    return false;
}

} // namespace planthub
