#pragma once

#include <functional>
#include <string>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/log.h"
#include "json_helper.h"
#include "provisioning_page.h"

namespace planthub {

class ProvisioningHandler : public AsyncWebHandler {
 public:
  ProvisioningHandler(std::string *state, std::string *claim_code, std::string *node_id,
                      const std::string &device_name)
      : state_(state), claim_code_(claim_code), node_id_(node_id), device_name_(device_name) {}

  void set_state_callback(std::function<void(const std::string &)> cb) { state_callback_ = std::move(cb); }

  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request == nullptr)
      return false;

    const auto method = request->method();
    const std::string url = request->url();

    if (url == "/setup") {
      return method == HTTP_GET;
    }
    if (url == "/scan") {
      return method == HTTP_POST || method == HTTP_GET;
    }
    if (url == "/wifisave") {
      return method == HTTP_POST || method == HTTP_GET;
    }
    if (url == "/provision-status") {
      return method == HTTP_POST || method == HTTP_GET;
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    if (request == nullptr) {
      return;
    }

    const std::string url = request->url();
    if (url == "/setup") {
      handle_setup_(request);
      return;
    }
    if (url == "/scan") {
      handle_scan_(request);
      return;
    }
    if (url == "/wifisave") {
      handle_wifisave_(request);
      return;
    }
    if (url == "/provision-status") {
      handle_status_(request);
      return;
    }

    request->send(404, "text/plain", "Not found");
  }

 private:
  void add_no_cache_headers_(AsyncWebServerResponse *response) {
    if (response == nullptr)
      return;
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
  }

  void handle_setup_(AsyncWebServerRequest *request) {
    const std::string state = state_ != nullptr ? *state_ : "";
    const std::string claim_code = claim_code_ != nullptr ? *claim_code_ : "";
    const std::string node_id = node_id_ != nullptr ? *node_id_ : "";

    ESP_LOGI("prov-ui", "/setup requested, state=%s claim_present=%s node_id=%s", state.c_str(),
             claim_code.empty() ? "false" : "true", node_id.c_str());

    std::string html = get_provisioning_page_html(state, claim_code, node_id, device_name_);
    auto *response = request->beginResponse(200, "text/html", html.c_str());
    add_no_cache_headers_(response);
    request->send(response);
  }

  void handle_scan_(AsyncWebServerRequest *request) {
    if (wifi::global_wifi_component == nullptr) {
      request->send(500, "application/json", "{\"aps\":[]}");
      return;
    }

    std::string json = "{\"aps\":[";
    bool first = true;
    for (auto &sr : wifi::global_wifi_component->get_scan_result()) {
      if (sr.get_is_hidden() || sr.get_ssid().empty()) {
        continue;
      }

      if (!first)
        json += ",";
      first = false;

      json += "{\"ssid\":\"" + escapeJson(sr.get_ssid()) + "\",\"rssi\":" +
              std::to_string(sr.get_rssi()) + ",\"lock\":" +
              std::to_string(sr.get_with_auth() ? 1 : 0) + "}";
    }
    json += "]}";

    auto *response = request->beginResponse(200, "application/json", json.c_str());
    add_no_cache_headers_(response);
    request->send(response);
  }

  void handle_wifisave_(AsyncWebServerRequest *request) {
    const std::string ssid = request->arg("ssid").c_str();
    const std::string psk = request->arg("psk").c_str();

    if (ssid.empty()) {
      request->send(400, "application/json", "{\"error\":\"missing ssid\"}");
      return;
    }

    ESP_LOGI("provisioning", "WiFi save from setup page: SSID='%s'", ssid.c_str());

    if (state_ != nullptr) {
      *state_ = "CONNECTING";
    }
    if (state_callback_) {
      state_callback_("CONNECTING");
    }

    if (wifi::global_wifi_component != nullptr) {
      wifi::global_wifi_component->save_wifi_sta(ssid, psk);
    }

    auto *response = request->beginResponse(200, "application/json", "{\"ok\":true}");
    add_no_cache_headers_(response);
    request->send(response);
  }

  void handle_status_(AsyncWebServerRequest *request) {
    const std::string state = state_ != nullptr ? *state_ : "";
    const std::string claim_code = claim_code_ != nullptr ? *claim_code_ : "";
    const std::string node_id = node_id_ != nullptr ? *node_id_ : "";

    ESP_LOGI("prov-api", "/provision-status called, state=%s claim_present=%s node_id=%s", state.c_str(),
             claim_code.empty() ? "false" : "true", node_id.c_str());

    const std::string body = "{\"state\":\"" + escapeJson(state) + "\",\"claimCode\":\"" +
                             escapeJson(claim_code) + "\",\"nodeId\":\"" + escapeJson(node_id) + "\"}";

    auto *response = request->beginResponse(200, "application/json", body.c_str());
    add_no_cache_headers_(response);
    request->send(response);
  }

  std::string *state_;
  std::string *claim_code_;
  std::string *node_id_;
  std::string device_name_;
  std::function<void(const std::string &)> state_callback_;
};

}  // namespace planthub
