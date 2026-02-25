#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"
class Application
{
private:
    /* data */
    Application();
    ~Application();
    static QueueHandle_t xQueueTrans;
    static QueueHandle_t xQueueTransOled;
    bool m_mqtt_connected;
    static void mpu6050(void *arg);
    static void mqtt_trans(void *arg);

public:
    static Application &getInstance()
    {
        static Application instance;
        return instance;
    };
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void Start();

    void onMQTTMessage(const std::string &topic, const std::string &data, int &data_len);
    void onMQTTConnection(bool connected);
    void onMQTTError(int error_type, void *error_data);
    
};

#endif // _APPLICATION_H_