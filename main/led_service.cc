#include "led_service.h"
#include "esp_log.h"
#include "math.h"

static const char *TAG = "LED_SERVICE_CPP";

// 单例实例
LedService &LedService::getInstance()
{
    static LedService instance;
    return instance;
}

LedService::LedService()
    : m_ledStrip(nullptr), m_ledEventQueue(nullptr), m_brightness(255), m_serviceInitialized(false), m_ledTaskHandle(nullptr), m_eventHandler(nullptr)
{
    // 构造函数
}

LedService::~LedService()
{
    if (m_serviceInitialized)
    {
        deinit();
    }
}

esp_err_t LedService::init(const LedConfig &config)
{
    // 保存配置
    m_ledConfig = config;

    // 创建事件队列
    m_ledEventQueue = xQueueCreate(10, sizeof(LedEvent));
    if (!m_ledEventQueue)
    {
        ESP_LOGE(TAG, "Failed to create LED event queue");
        return ESP_FAIL;
    }

    // LED灯带通用配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = config.gpio,
        .max_leds = config.num_leds,
        .led_model = config.model,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false, // 不反向输出信号
        }};

    // RMT后端特定配置
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false, // 如果LED数量很多，可以启用DMA
        }};

    // 创建LED灯带句柄
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &m_ledStrip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create LED strip device");
        return ret;
    }

    // 清空灯带（确保所有LED熄灭）
    led_strip_clear(m_ledStrip);

    // 创建LED服务任务
    BaseType_t task_ret = xTaskCreate(
        LedService::ledServiceTask,
        "led_service_cpp_task",
        4096,
        this, // 传递this指针
        5,
        &m_ledTaskHandle);

    if (task_ret != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to create LED service task");
        led_strip_del(m_ledStrip);
        m_ledStrip = nullptr;
        vQueueDelete(m_ledEventQueue);
        m_ledEventQueue = nullptr;
        return ESP_FAIL;
    }

    m_serviceInitialized = true;
    ESP_LOGI(TAG, "LED service initialized successfully");
    return ESP_OK;
}

esp_err_t LedService::deinit()
{
    if (!m_serviceInitialized)
    {
        ESP_LOGW(TAG, "LED service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 删除任务
    if (m_ledTaskHandle)
    {
        vTaskDelete(m_ledTaskHandle);
        m_ledTaskHandle = nullptr;
    }

    // 删除队列
    if (m_ledEventQueue)
    {
        vQueueDelete(m_ledEventQueue);
        m_ledEventQueue = nullptr;
    }

    // 删除LED设备
    if (m_ledStrip)
    {
        led_strip_del(m_ledStrip);
        m_ledStrip = nullptr;
    }

    m_serviceInitialized = false;
    ESP_LOGI(TAG, "LED service deinitialized");
    return ESP_OK;
}

esp_err_t LedService::triggerEvent(LedEvent event)
{
    if (!m_ledEventQueue)
    {
        ESP_LOGE(TAG, "LED event queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 尝试发送事件到队列
    BaseType_t ret = xQueueSend(m_ledEventQueue, &event, 0);
    if (ret != pdTRUE)
    {
        ESP_LOGW(TAG, "Failed to send event to LED queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t LedService::setCustomColor(uint32_t color)
{
    // 这里可以实现设置自定义颜色的功能
    // 实际应用中，你可能需要存储这个颜色值供后续使用
    // ESP_LOGI(TAG, "Custom color set: 0x%u", color);
    return ESP_OK;
}

esp_err_t LedService::setBrightness(uint8_t brightness)
{
    m_brightness = brightness;
    ESP_LOGD(TAG, "Brightness set to %d", brightness);
    return ESP_OK;
}

uint8_t LedService::getBrightness() const
{
    return m_brightness;
}

bool LedService::isInitialized() const
{
    return m_serviceInitialized;
}

void LedService::registerEventHandler(std::function<void(LedEvent)> handler)
{
    m_eventHandler = handler;
}

void LedService::ledServiceTask(void *arg)
{
    LedService *self = static_cast<LedService *>(arg);
    LedEvent event;

    while (1)
    {
        if (xQueueReceive(self->m_ledEventQueue, &event, portMAX_DELAY) == pdTRUE)
        {
            self->handleLedEvent(event);
        }
    }
}

void LedService::handleLedEvent(LedEvent event)
{
    if (!m_ledStrip)
    {
        ESP_LOGE(TAG, "LED strip not initialized");
        return;
    }

    // 如果注册了自定义事件处理器，先调用它
    if (m_eventHandler)
    {
        m_eventHandler(event);
    }

    switch (event)
    {
    case LedEvent::PowerOn:
        ESP_LOGI(TAG, "LedEvent::PowerOn triggered");
        // 开机效果：彩虹色
        rainbowEffect();
        break;

    case LedEvent::WifiConnect:
        ESP_LOGI(TAG, "LedEvent::WifiConnect triggered");
        // WiFi连接成功：绿色常亮
        setAllLeds(0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        clearAllLeds();
        break;

    case LedEvent::WifiDisconnect:
        ESP_LOGI(TAG, "LedEvent::WifiDisconnect triggered");
        // WiFi断开连接：红色闪烁3次
        blinkEffect(255, 0, 0, 3, 200);
        break;

    case LedEvent::MqttConnect:
        ESP_LOGI(TAG, "LedEvent::MqttConnect triggered");
        // MQTT连接成功：青色常亮
        setAllLeds(0, 255, 255);
        vTaskDelay(pdMS_TO_TICKS(1000));
        clearAllLeds();
        break;

    case LedEvent::MqttDisconnect:
        ESP_LOGI(TAG, "LedEvent::MqttDisconnect triggered");
        // MQTT断开连接：橙色闪烁3次
        blinkEffect(255, 90, 0, 3, 200);
        break;

    case LedEvent::BleConnect:
        ESP_LOGI(TAG, "LedEvent::BleConnect triggered");
        // BLE连接成功：紫色常亮
        setAllLeds(128, 0, 128);
        vTaskDelay(pdMS_TO_TICKS(1000));
        clearAllLeds();
        break;

    case LedEvent::BleDisconnect:
        ESP_LOGI(TAG, "LedEvent::BleDisconnect triggered");
        // BLE断开连接：紫色闪烁3次
        blinkEffect(128, 0, 128, 3, 200);
        break;

    case LedEvent::GameStart:
        ESP_LOGI(TAG, "LedEvent::GameStart triggered");
        // 游戏开始：黄色流水灯
        chaseEffect(255, 255, 0);
        break;

    case LedEvent::GameEnd:
        ESP_LOGI(TAG, "LedEvent::GameEnd triggered");
        // 游戏结束：白色流水灯
        chaseEffect(255, 255, 255);
        break;

    case LedEvent::PlayerJoin:
        ESP_LOGI(TAG, "LedEvent::PlayerJoin triggered");
        // 玩家加入：绿色闪烁2次
        blinkEffect(0, 255, 0, 2, 200);
        break;

    case LedEvent::PlayerLeave:
        ESP_LOGI(TAG, "LedEvent::PlayerLeave triggered");
        // 玩家离开：红色闪烁2次
        blinkEffect(255, 0, 0, 2, 200);
        break;

    case LedEvent::Victory:
        ESP_LOGI(TAG, "LedEvent::Victory triggered");
        // 胜利：金色呼吸灯效果
        breathingEffect(255, 215, 0);
        break;

    case LedEvent::Defeat:
        ESP_LOGI(TAG, "LedEvent::Defeat triggered");
        // 失败：红色呼吸灯效果
        breathingEffect(255, 0, 0);
        break;

    case LedEvent::Alert:
        ESP_LOGI(TAG, "LedEvent::Alert triggered");
        // 警报：红蓝交替闪烁
        for (int i = 0; i < 5; i++)
        {
            setAllLeds(255, 0, 0); // 红色
            vTaskDelay(pdMS_TO_TICKS(200));
            setAllLeds(0, 0, 255); // 蓝色
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        clearAllLeds();
        break;

    case LedEvent::Custom:
        ESP_LOGI(TAG, "LedEvent::Custom triggered");
        // 自定义事件：由外部设置的颜色
        // 注意：实际应用中，这里应该使用存储的自定义颜色
        setAllLeds(128, 128, 128); // 默认灰色
        vTaskDelay(pdMS_TO_TICKS(500));
        clearAllLeds();
        break;

    case LedEvent::Off:
        ESP_LOGI(TAG, "LedEvent::Off triggered");
        // 关闭LED
        clearAllLeds();
        break;

    default:
        ESP_LOGW(TAG, "Unknown LED event");
        break;
    }
}

void LedService::setAllLeds(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int i = 0; i < m_ledConfig.num_leds; i++)
    {
        led_strip_set_pixel(m_ledStrip, i, applyBrightness(red), applyBrightness(green), applyBrightness(blue));
    }
    led_strip_refresh(m_ledStrip);
}

void LedService::clearAllLeds()
{
    for (int i = 0; i < m_ledConfig.num_leds; i++)
    {
        led_strip_set_pixel(m_ledStrip, i, 0, 0, 0);
    }
    led_strip_refresh(m_ledStrip);
}

void LedService::blinkEffect(uint8_t red, uint8_t green, uint8_t blue, int times, int delayMs)
{
    for (int i = 0; i < times; i++)
    {
        setAllLeds(red, green, blue);
        vTaskDelay(pdMS_TO_TICKS(delayMs));
        clearAllLeds();
        if (i < times - 1)
        { // 最后一次不需要等待
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
}

void LedService::chaseEffect(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int cycle = 0; cycle < 3; cycle++)
    {
        for (int i = 0; i < m_ledConfig.num_leds; i++)
        {
            clearAllLeds();
            led_strip_set_pixel(m_ledStrip, i, applyBrightness(red), applyBrightness(green), applyBrightness(blue));
            led_strip_refresh(m_ledStrip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        for (int i = m_ledConfig.num_leds - 1; i >= 0; i--)
        {
            clearAllLeds();
            led_strip_set_pixel(m_ledStrip, i, applyBrightness(red), applyBrightness(green), applyBrightness(blue));
            led_strip_refresh(m_ledStrip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    clearAllLeds();
}

void LedService::rainbowChaseEffect()
{
    for (int cycle = 0; cycle < 3; cycle++)
    {
        for (int i = 0; i < m_ledConfig.num_leds; i++)
        {
            clearAllLeds();
            HsvColor hsv = {(float)(i * 360 / m_ledConfig.num_leds), 1.0f, 1.0f};
            RgbColor rgb = hsvToRgb(hsv);
            led_strip_set_pixel(m_ledStrip, i, applyBrightness(rgb.r), applyBrightness(rgb.g), applyBrightness(rgb.b));
            led_strip_refresh(m_ledStrip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        clearAllLeds();
        led_strip_refresh(m_ledStrip);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void LedService::waveEffect(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int cycle = 0; cycle < 3; cycle++)
    {
        // 正向波浪
        for (int pos = 0; pos < m_ledConfig.num_leds + 5; pos++)
        {
            clearAllLeds();
            for (int i = 0; i < m_ledConfig.num_leds; i++)
            {
                float distance = fabsf(i - pos);
                if (distance < 3)
                { // 在波峰附近点亮LED
                    float intensity = 1.0f - (distance / 3.0f);
                    led_strip_set_pixel(m_ledStrip, i,
                                        applyBrightness((uint8_t)(red * intensity)),
                                        applyBrightness((uint8_t)(green * intensity)),
                                        applyBrightness((uint8_t)(blue * intensity)));
                }
            }
            led_strip_refresh(m_ledStrip);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // 反向波浪
        for (int pos = m_ledConfig.num_leds + 5; pos >= -5; pos--)
        {
            clearAllLeds();
            for (int i = 0; i < m_ledConfig.num_leds; i++)
            {
                float distance = fabsf(i - pos);
                if (distance < 3)
                { // 在波峰附近点亮LED
                    float intensity = 1.0f - (distance / 3.0f);
                    led_strip_set_pixel(m_ledStrip, i,
                                        applyBrightness((uint8_t)(red * intensity)),
                                        applyBrightness((uint8_t)(green * intensity)),
                                        applyBrightness((uint8_t)(blue * intensity)));
                }
            }
            led_strip_refresh(m_ledStrip);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    clearAllLeds();
}

void LedService::rainbowEffect()
{
    for (int hue = 0; hue < 360; hue += 10)
    {
        for (int i = 0; i < m_ledConfig.num_leds; i++)
        {
            HsvColor hsv = {(float)(hue + (i * 60)), 1.0f, 1.0f};
            RgbColor rgb = hsvToRgb(hsv);
            led_strip_set_pixel(m_ledStrip, i, applyBrightness(rgb.r), applyBrightness(rgb.g), applyBrightness(rgb.b));
        }
        led_strip_refresh(m_ledStrip);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    clearAllLeds();
}

void LedService::breathingEffect(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int i = 0; i <= 255; i += 5)
    {
        setAllLeds((red * i) / 255, (green * i) / 255, (blue * i) / 255);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    for (int i = 255; i >= 0; i -= 5)
    {
        setAllLeds((red * i) / 255, (green * i) / 255, (blue * i) / 255);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    clearAllLeds();
}

LedService::RgbColor LedService::hsvToRgb(HsvColor hsv)
{
    RgbColor rgb = {0, 0, 0};

    if (hsv.s == 0)
    {
        // 饱和度为0，是灰度颜色
        rgb.r = rgb.g = rgb.b = (uint8_t)(hsv.v * 255);
        return rgb;
    }

    float h = hsv.h / 60.0f;
    int i = (int)floorf(h);
    float f = h - i;
    float p = hsv.v * (1 - hsv.s);
    float q = hsv.v * (1 - hsv.s * f);
    float t = hsv.v * (1 - hsv.s * (1 - f));

    switch (i % 6)
    {
    case 0:
        rgb.r = (uint8_t)(hsv.v * 255);
        rgb.g = (uint8_t)(t * 255);
        rgb.b = (uint8_t)(p * 255);
        break;
    case 1:
        rgb.r = (uint8_t)(q * 255);
        rgb.g = (uint8_t)(hsv.v * 255);
        rgb.b = (uint8_t)(p * 255);
        break;
    case 2:
        rgb.r = (uint8_t)(p * 255);
        rgb.g = (uint8_t)(hsv.v * 255);
        rgb.b = (uint8_t)(t * 255);
        break;
    case 3:
        rgb.r = (uint8_t)(p * 255);
        rgb.g = (uint8_t)(q * 255);
        rgb.b = (uint8_t)(hsv.v * 255);
        break;
    case 4:
        rgb.r = (uint8_t)(t * 255);
        rgb.g = (uint8_t)(p * 255);
        rgb.b = (uint8_t)(hsv.v * 255);
        break;
    case 5:
        rgb.r = (uint8_t)(hsv.v * 255);
        rgb.g = (uint8_t)(p * 255);
        rgb.b = (uint8_t)(q * 255);
        break;
    }

    return rgb;
}

uint8_t LedService::applyBrightness(uint8_t value)
{
    return (uint8_t)((value * m_brightness) / 255);
}