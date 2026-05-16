#pragma once

#include <string>
#include <functional>
#include <mqtt_client.h>
#include "esphome/core/log.h"

namespace planthub {

// Amazon Root CA 1 - baked into firmware
static const char* AMAZON_ROOT_CA_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

class PlantHubMqtt {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    PlantHubMqtt();
    bool setup();
    bool start();
    void stop();
    int publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
    bool subscribe(const std::string& topic, MessageCallback callback, int qos = 1);
    void set_callback(MessageCallback callback) { callback_ = callback; }
    void set_shadow_callback(MessageCallback callback) { shadow_callback_ = callback; }
    void set_rules_config_callback(MessageCallback callback) { rules_config_callback_ = callback; }
    bool is_connected() const;
    bool is_aws_iot_core() const { return provider_ == "aws-iot-core"; }

    const std::string& get_sensor_topic() const { return sensor_topic_; }
    const std::string& get_actuator_topic() const { return actuator_topic_; }
    const std::string& get_status_topic() const { return status_topic_; }
    const std::string& get_capabilities_topic() const { return capabilities_topic_; }
    const std::string& get_base_topic() const { return base_topic_; }
    const std::string& get_system_topic() const { return system_topic_; }
    const std::string& get_actuator_state_topic() const { return actuator_state_topic_; }
    const std::string& get_maintenance_topic() const { return maintenance_topic_; }
    const std::string& get_rules_config_topic() const { return rules_config_topic_; }
    const std::string& get_shadow_get_topic() const { return shadow_get_topic_; }
    const std::string& get_shadow_update_topic() const { return shadow_update_topic_; }
    const std::string& get_node_id() const { return client_id_; }
    /**
     * The AWS IoT Thing name — derived as "{tenantId}-{nodeId}" during
     * setup(). Different from the MQTT clientId (which equals nodeId) and
     * different from the tenant/node MQTT topic prefix. Use this when
     * composing $aws/things/.../shadow/... ARNs.
     */
    const std::string& get_thing_name() const { return thing_name_; }

    /**
     * Publish an empty {} to $aws/things/{node}/shadow/name/config/get
     * triggering the broker to respond on get/accepted with the current
     * desired state. Safe no-op on non-aws-iot-core providers.
     */
    int request_shadow_get();

    void publish_birth();
    void publish_will();

private:
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    
    esp_mqtt_client_handle_t client_;
    bool connected_;
    std::string client_id_;
    std::string cert_pem_;
    std::string key_pem_;
    std::string endpoint_;
    std::string broker_url_;
    std::string mqtt_username_;
    std::string mqtt_password_;
    std::string provider_;
    std::string base_topic_;
    std::string sensor_topic_;
    std::string actuator_topic_;
    std::string status_topic_;
    std::string capabilities_topic_;
    std::string system_topic_;
    std::string actuator_state_topic_;
    std::string maintenance_topic_;
    // Retained backend → device rules feed. Carries the canonical compact
    // rules JSON ({"rules":[...],"rulesHash":"..."}) that used to live inside
    // the shadow desired-state. Decoupling rules from the shadow stops AWS
    // from re-emitting update/delta forever when the device echoes back only
    // the hash.
    std::string rules_config_topic_;
    std::string thing_name_;
    std::string shadow_get_topic_;
    std::string shadow_get_accepted_topic_;
    std::string shadow_update_topic_;
    std::string shadow_update_delta_topic_;
    std::string subscribe_topic_;
    MessageCallback callback_;
    MessageCallback shadow_callback_;
    MessageCallback rules_config_callback_;
};

extern PlantHubMqtt planthub_mqtt;

} // namespace planthub
