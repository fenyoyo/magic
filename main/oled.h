#ifndef _OLED_H_
#define _OLED_H_

#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string>

struct PoseData
{
    uint32_t seq;
    int16_t ax, ay, az; // 加速度计数据
    int16_t gx, gy, gz; // 陀螺仪数据
};

class OLED
{
private:
    ssd1306_handle_t m_ssd1306_dev;
    bool m_initialized;
    QueueHandle_t m_display_queue;
    TaskHandle_t m_task_handle;

    static void oled_task(void *arg);

public:
    OLED();
    ~OLED();

    bool initialize();
    void deinitialize();

    bool display_gyro_data(const PoseData &pose);
    bool display_message(const std::string &msg, uint8_t font_size = 16);
    void clear_screen();
    void refresh();

    bool is_initialized() const { return m_initialized; }
};

#endif // _OLED_H_