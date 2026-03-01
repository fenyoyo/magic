#include "oled.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "string.h"
#include <cstdio>

#define OLED_TAG "OLED"
#define SSD1306_I2C_ADDRESS ((uint8_t)0x3C)
#define I2C_MASTER_SCL_IO 17      /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 18      /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */

OLED::OLED() : m_ssd1306_dev(nullptr), m_initialized(false), m_display_queue(nullptr), m_task_handle(nullptr)
{
}

OLED::~OLED()
{
    deinitialize();
}

bool OLED::initialize()
{
    if (m_initialized)
    {
        return true;
    }

    // 创建显示队列
    m_display_queue = xQueueCreate(10, sizeof(PoseData));
    if (m_display_queue == nullptr)
    {
        ESP_LOGE(OLED_TAG, "Failed to create display queue");
        return false;
    }

    // 初始化I2C
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(OLED_TAG, "Failed to configure I2C");
        return false;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(OLED_TAG, "Failed to install I2C driver");
        return false;
    }

    // 初始化SSD1306 OLED显示屏
    m_ssd1306_dev = ssd1306_create(I2C_MASTER_NUM, SSD1306_I2C_ADDRESS);
    if (m_ssd1306_dev == nullptr)
    {
        ESP_LOGE(OLED_TAG, "Failed to create SSD1306 device handle");
        return false;
    }

    ret = ssd1306_init(m_ssd1306_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(OLED_TAG, "Failed to initialize SSD1306: %d", ret);
        ssd1306_delete(m_ssd1306_dev);
        m_ssd1306_dev = nullptr;
        return false;
    }

    ESP_LOGI(OLED_TAG, "SSD1306 initialized successfully");

    // // 设置对比度以改善显示清晰度
    // uint8_t contrast_cmd[] = {0x81, 0xFF}; // 设置对比度为最大值
    // ssd1306_write_cmd(m_ssd1306_dev, contrast_cmd, sizeof(contrast_cmd));

    // // 设置预充电周期
    // uint8_t precharge_cmd[] = {0xD9, 0xF1};
    // ssd1306_write_cmd(m_ssd1306_dev, precharge_cmd, sizeof(precharge_cmd));

    // // 设置VCOMH去选中级别
    // uint8_t vcomh_cmd[] = {0xDB, 0x40};
    // ssd1306_write_cmd(m_ssd1306_dev, vcomh_cmd, sizeof(vcomh_cmd));

    // // 设置显示开/关 (确保显示开启)
    // uint8_t display_on_cmd[] = {0xAF}; // 开启显示
    // ssd1306_write_cmd(m_ssd1306_dev, display_on_cmd, sizeof(display_on_cmd));

    // 启动OLED显示任务
    BaseType_t task_ret = xTaskCreate(&OLED::oled_task, "OLED_TASK", 1024 * 4, this, 4, &m_task_handle);
    if (task_ret != pdPASS)
    {
        ESP_LOGE(OLED_TAG, "Failed to create OLED task");
        ssd1306_delete(m_ssd1306_dev);
        m_ssd1306_dev = nullptr;
        vQueueDelete(m_display_queue);
        m_display_queue = nullptr;
        return false;
    }

    m_initialized = true;
    return true;
}

void OLED::deinitialize()
{
    if (m_task_handle != nullptr)
    {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }

    if (m_display_queue != nullptr)
    {
        vQueueDelete(m_display_queue);
        m_display_queue = nullptr;
    }

    if (m_ssd1306_dev != nullptr)
    {
        ssd1306_delete(m_ssd1306_dev);
        m_ssd1306_dev = nullptr;
    }

    m_initialized = false;
}

bool OLED::display_gyro_data(const PoseData &pose)
{
    if (!m_initialized || m_display_queue == nullptr)
    {
        return false;
    }

    BaseType_t ret = xQueueSend(m_display_queue, &pose, 0);
    return (ret == pdTRUE);
}

bool OLED::display_message(const std::string &msg, uint8_t font_size)
{
    if (!m_initialized || m_ssd1306_dev == nullptr)
    {
        return false;
    }

    clear_screen();

    if (msg.length() > 0)
    {
        ssd1306_draw_string(m_ssd1306_dev, 0, 0, (const uint8_t *)msg.c_str(), font_size, 1);
    }

    refresh();
    return true;
}

void OLED::clear_screen()
{
    if (!m_initialized || m_ssd1306_dev == nullptr)
    {
        return;
    }
    ssd1306_clear_screen(m_ssd1306_dev, 0x00);
}

void OLED::refresh()
{
    if (!m_initialized || m_ssd1306_dev == nullptr)
    {
        return;
    }
    ssd1306_refresh_gram(m_ssd1306_dev);
}

void OLED::oled_task(void *arg)
{
    OLED *oled = static_cast<OLED *>(arg);
    if (oled == nullptr)
    {
        ESP_LOGE(OLED_TAG, "OLED task: invalid argument");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(OLED_TAG, "OLED Display Task Started");

    // 初始化显示
    if (oled->m_ssd1306_dev != nullptr)
    {
        // oled->clear_screen();
        // char title_str[20] = "Migic Wand";
        // ssd1306_draw_string(oled->m_ssd1306_dev, 10, 0, (const uint8_t *)title_str, 16, 1);
        // oled->refresh();
    }

    PoseData pose;
    char buffer[64];
    bool has_received_data = false; // 跟踪是否收到过数据

    while (1)
    {
        if (xQueueReceive(oled->m_display_queue, &pose, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            has_received_data = true;

            // 清除屏幕特定区域用于显示数据
            if (oled->m_ssd1306_dev != nullptr)
            {
                ssd1306_clear_screen(oled->m_ssd1306_dev, 0x00); // 先清除整个屏幕再重新绘制
                oled->refresh();
                // 重新绘制标题
                // char title_str[20] = "GYRO DATA";
                // ssd1306_draw_string(oled->m_ssd1306_dev, 10, 0, (const uint8_t *)title_str, 16, 1);

                // 显示加速度数据 (ax, ay, az) - 分两行显示，节省空间
                snprintf(buffer, sizeof(buffer), "A:%d,%d", pose.ax, pose.ay);
                ssd1306_draw_string(oled->m_ssd1306_dev, 0, 20, (const uint8_t *)buffer, 16, 1); // 使用16号字体，更清晰

                oled->refresh();
            }
        }
        else
        {
            // 如果队列为空但之前收到过数据，保持最后数据显示一段时间
            if (has_received_data)
            {
                // 在这里可以选择保持最后一次数据显示而不清屏
                // 或者添加一些动态效果表明正在等待新数据
                vTaskDelay(pdMS_TO_TICKS(50)); // 短暂延迟，避免CPU占用过高
            }
            else
            {
                // 如果从未收到数据，显示等待信息
                // if (oled->m_ssd1306_dev != nullptr)
                // {
                //     oled->clear_screen();
                //     ssd1306_draw_string(oled->m_ssd1306_dev, 10, 0, (const uint8_t *)"Migic Wand", 16, 1);
                //     ssd1306_draw_string(oled->m_ssd1306_dev, 15, 40, (const uint8_t *)"Waiting...", 16, 1); // 使用16号字体，更清晰
                //     oled->refresh();
                //     vTaskDelay(pdMS_TO_TICKS(100)); // 等待状态下稍微延长延迟
                // }
            }
        }
    }

    vTaskDelete(nullptr);
}