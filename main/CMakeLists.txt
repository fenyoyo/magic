idf_component_register(SRCS "main.cc"
                    PRIV_REQUIRES esp_wifi nvs_flash freertos mqtt
                    INCLUDE_DIRS ".")
