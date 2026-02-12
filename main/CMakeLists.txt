idf_component_register(SRCS "main.cc" "mqtt_manager.cc" "application.cc" "board.cc" "wifi_manager.cc"
                    PRIV_REQUIRES esp_wifi nvs_flash freertos mqtt
                    INCLUDE_DIRS ".")
