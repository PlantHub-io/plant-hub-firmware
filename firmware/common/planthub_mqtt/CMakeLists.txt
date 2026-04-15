idf_component_register(
    SRCS "planthub_mqtt.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash mqtt
)
