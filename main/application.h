#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"
#include "person_detect_model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

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

    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    int inference_count = 0;

    static constexpr int kTensorArenaSize = 200 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
};

#endif // _APPLICATION_H_