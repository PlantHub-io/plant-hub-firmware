#pragma once

#include <string>
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

namespace planthub {
  static const char* NVS_NAMESPACE = "planthub";
  // Use inline to ensure a single instance across all translation units.
  // 'static' would give each .cpp file its own copy, causing planthub_mqtt.cpp
  // to use an uninitialized handle when it calls nvs_read().
  inline nvs_handle_t nvs_handle = 0;
  inline bool nvs_initialized = false;

  inline bool nvs_init() {
    if (nvs_initialized) return true;
    
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      return false;
    }
    
    nvs_initialized = true;
    return true;
  }

  inline std::string nvs_read(const char* key) {
    if (!nvs_initialized) return "";
    
    size_t length = 0;
    esp_err_t err = nvs_get_str(nvs_handle, key, NULL, &length);
    if (err != ESP_OK || length == 0) {
      return "";
    }
    
    char* buffer = new char[length];
    err = nvs_get_str(nvs_handle, key, buffer, &length);
    if (err != ESP_OK) {
      delete[] buffer;
      return "";
    }
    
    std::string result(buffer);
    delete[] buffer;
    return result;
  }

  inline bool nvs_write(const char* key, const std::string& value) {
    if (!nvs_initialized) return false;
    esp_err_t err = nvs_set_str(nvs_handle, key, value.c_str());
    if (err != ESP_OK) {
      ESP_LOGE("nvs", "Failed to write key '%s' (%d bytes): %s", key, value.length(), esp_err_to_name(err));
      return false;
    }
    return true;
  }

  inline void nvs_save() {
    if (!nvs_initialized) return;
    nvs_commit(nvs_handle);
  }

  inline void nvs_clear() {
    if (!nvs_initialized) return;
    nvs_erase_all(nvs_handle);
    nvs_commit(nvs_handle);
  }

  inline bool nvs_has_key(const char* key) {
    if (!nvs_initialized) return false;
    size_t length = 0;
    return nvs_get_str(nvs_handle, key, NULL, &length) == ESP_OK;
  }

  inline void nvs_delete(const char* key) {
    if (!nvs_initialized) return;
    nvs_erase_key(nvs_handle, key);
  }

  inline void nvs_log_free_space() {
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    if (err == ESP_OK) {
        ESP_LOGI("nvs", "NVS Stats: UsedEntries=%d, FreeEntries=%d, TotalEntries=%d, NamespaceCount=%d",
                 nvs_stats.used_entries, nvs_stats.free_entries,
                 nvs_stats.total_entries, nvs_stats.namespace_count);
    } else {
        ESP_LOGE("nvs", "Failed to get NVS stats: %s", esp_err_to_name(err));
    }
  }
}
