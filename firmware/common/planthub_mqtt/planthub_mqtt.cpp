#include "planthub_mqtt.h"
#include "nvs_helper.h"

namespace planthub {

PlantHubMqtt::PlantHubMqtt() : client_(nullptr), connected_(false), callback_(nullptr) {}

bool PlantHubMqtt::setup() {
    ESP_LOGI("mqtt", "Initializing PlantHub MQTT...");

    std::string provider = planthub::nvs_read("mqtt_provider");
    std::string node_id = planthub::nvs_read("node_id");
    std::string tenant_id = planthub::nvs_read("tenant_id");

    if (provider.empty() || node_id.empty()) {
        ESP_LOGE("mqtt", "Missing MQTT configuration in NVS");
        return false;
    }

    client_id_ = node_id;

    esp_mqtt_client_config_t config = {};

    if (provider == "aws-iot-core") {
        endpoint_ = planthub::nvs_read("mqtt_endpoint");
        cert_pem_ = planthub::nvs_read("mqtt_cert");
        key_pem_ = planthub::nvs_read("mqtt_key");

        if (endpoint_.empty() || cert_pem_.empty() || key_pem_.empty()) {
            ESP_LOGE("mqtt", "Missing AWS IoT configuration in NVS");
            return false;
        }

        config.broker.address.hostname = endpoint_.c_str();
        config.broker.address.port = 8883;
        config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
        config.broker.verification.certificate = AMAZON_ROOT_CA_PEM;
        config.credentials.authentication.certificate = cert_pem_.c_str();
        config.credentials.authentication.key = key_pem_.c_str();

        ESP_LOGI("mqtt", "Configured for AWS IoT Core: %s", endpoint_.c_str());
    } else {
        broker_url_ = planthub::nvs_read("mqtt_broker_url");
        if (broker_url_.empty()) {
            broker_url_ = "mqtt://localhost:1883";
        }

        config.broker.address.uri = broker_url_.c_str();

        mqtt_username_ = planthub::nvs_read("mqtt_username");
        mqtt_password_ = planthub::nvs_read("mqtt_password");
        if (!mqtt_username_.empty()) {
            config.credentials.username = mqtt_username_.c_str();
            config.credentials.authentication.password = mqtt_password_.c_str();
        }

        ESP_LOGI("mqtt", "Configured for Mosquitto: %s", broker_url_.c_str());
    }

    config.credentials.client_id = client_id_.c_str();
    config.session.keepalive = 60;
    config.session.disable_clean_session = false;

    if (!tenant_id.empty()) {
        base_topic_ = tenant_id + "/" + node_id;
        sensor_topic_ = base_topic_ + "/sensors";
        actuator_topic_ = base_topic_ + "/actuators";
        status_topic_ = base_topic_ + "/status";
        capabilities_topic_ = base_topic_ + "/capabilities";
        system_topic_ = base_topic_ + "/system";
        actuator_state_topic_ = base_topic_ + "/actuator-state";
        // Subscribe only to topics we expect commands on to avoid self-echoing
        subscribe_topic_ = "";

        // Set Last Will and Testament (LWT) to broadcast offline immediately
        config.session.last_will.topic = status_topic_.c_str();
        config.session.last_will.msg = "offline";
        config.session.last_will.msg_len = 7;
        config.session.last_will.qos = 1;
        config.session.last_will.retain = 1;
    }

    client_ = esp_mqtt_client_init(&config);
    if (client_ == nullptr) {
        ESP_LOGE("mqtt", "Failed to initialize MQTT client");
        return false;
    }

    esp_mqtt_client_register_event(client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), 
                                     &PlantHubMqtt::mqtt_event_handler, this);

    return true;
}

bool PlantHubMqtt::start() {
    if (client_ == nullptr) {
        ESP_LOGE("mqtt", "MQTT client not initialized, call setup() first");
        return false;
    }

    esp_err_t err = esp_mqtt_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE("mqtt", "Failed to start MQTT client: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI("mqtt", "MQTT client started");
    return true;
}

void PlantHubMqtt::stop() {
    if (client_ != nullptr) {
        esp_mqtt_client_stop(client_);
    }
}

int PlantHubMqtt::publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!connected_ || client_ == nullptr) {
        ESP_LOGW("mqtt", "Cannot publish, not connected");
        return -1;
    }

    return esp_mqtt_client_publish(client_, topic.c_str(), payload.c_str(), 
                                    payload.length(), qos, retain);
}

bool PlantHubMqtt::subscribe(const std::string& topic, MessageCallback callback, int qos) {
    if (client_ == nullptr) {
        ESP_LOGE("mqtt", "Cannot subscribe, client not initialized");
        return false;
    }

    callback_ = callback;
    subscribe_topic_ = topic;

    int msg_id = esp_mqtt_client_subscribe(client_, topic.c_str(), qos);
    if (msg_id < 0) {
        ESP_LOGE("mqtt", "Failed to subscribe to %s", topic.c_str());
        return false;
    }

    ESP_LOGI("mqtt", "Subscribed to %s", topic.c_str());
    return true;
}

bool PlantHubMqtt::is_connected() const {
    return connected_;
}

void PlantHubMqtt::publish_birth() {
    if (!status_topic_.empty()) {
        publish(status_topic_, "online", 1, true);
    }
}

void PlantHubMqtt::publish_will() {
    if (!status_topic_.empty()) {
        publish(status_topic_, "offline", 1, true);
    }
}

void PlantHubMqtt::mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                                       int32_t event_id, void* event_data) {
    PlantHubMqtt* self = static_cast<PlantHubMqtt*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("mqtt", "MQTT connected");
            self->connected_ = true;
            self->publish_birth();
            if (!self->base_topic_.empty() && self->callback_) {
                esp_mqtt_client_subscribe(self->client_, self->actuator_topic_.c_str(), 1);
                esp_mqtt_client_subscribe(self->client_, self->system_topic_.c_str(), 1);
            }
            if (!self->subscribe_topic_.empty() && self->callback_) {
                esp_mqtt_client_subscribe(self->client_, self->subscribe_topic_.c_str(), 1);
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW("mqtt", "MQTT disconnected");
            self->connected_ = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI("mqtt", "MQTT subscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI("mqtt", "MQTT unsubscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD("mqtt", "MQTT published, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            if (event->topic && event->data) {
                std::string topic(event->topic, event->topic_len);
                std::string payload(event->data, event->data_len);
                ESP_LOGI("mqtt", "Received on %s: %s", topic.c_str(), payload.c_str());
                if (self->callback_) {
                    self->callback_(topic, payload);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE("mqtt", "MQTT error");
            break;

        default:
            break;
    }
}

// Global instance
PlantHubMqtt planthub_mqtt;

} // namespace planthub
