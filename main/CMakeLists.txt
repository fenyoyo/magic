idf_component_register(SRCS "main.cc" "mqtt_manager.cc" "application.cc" "board.cc" "wifi_manager.cc" "mpu6050_sensor.cc" "oled_display.c"
                    PRIV_REQUIRES esp_wifi nvs_flash freertos mqtt driver mpu6050 i2cdev
                    INCLUDE_DIRS ".")
