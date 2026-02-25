# 主组件
idf_component_register(SRCS "gyro_predictor.cc" "main.cc" "mqtt_manager.cc" "application.cc" "board.cc" "wifi_manager.cc" "oled.cc"
                    PRIV_REQUIRES esp_wifi nvs_flash freertos mqtt driver esp-tflite-micro
                    REQUIRES MPU6050
                    INCLUDE_DIRS ".")

# 如果需要构建测试程序，则添加测试构建规则
# 注释掉以下内容以避免构建问题，如果需要可以取消注释
#idf_component_register(SRCS "test_filter.cc" "sliding_average_filter.cc"
#                    PRIV_REQUIRES 
#                    INCLUDE_DIRS ".")

# idf_component_register(SRCS "MPU6050.cpp"
#                     INCLUDE_DIRS "include")