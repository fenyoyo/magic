#pragma once

#include "esp_err.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <functional>

// 定义 LED 事件类型
enum class LedEvent
{
    PowerOn,        // 上电事件
    WifiConnect,    // WiFi连接成功
    WifiDisconnect, // WiFi断开连接
    MqttConnect,    // MQTT连接成功
    MqttDisconnect, // MQTT断开连接
    BleConnect,     // BLE连接成功
    BleDisconnect,  // BLE断开连接
    GameStart,      // 游戏开始
    GameEnd,        // 游戏结束
    PlayerJoin,     // 玩家加入
    PlayerLeave,    // 玩家离开
    Victory,        // 胜利
    Defeat,         // 失败
    Alert,          // 警报
    Custom,         // 自定义事件
    Off             // 关闭LED
};

struct LedConfig
{
    int gpio;           // LED控制GPIO
    uint32_t num_leds;  // LED数量
    led_model_t model;  // LED模型 (WS2812, SK6812等)
    bool invert_output; // 是否反转输出
};

class LedService
{
public:
    // 单例模式获取实例
    static LedService &getInstance();

    // 初始化LED服务
    esp_err_t init(const LedConfig &config);

    // 停止LED服务
    esp_err_t deinit();

    // 触发LED事件
    esp_err_t triggerEvent(LedEvent event);

    // 设置自定义颜色 (用于LedEvent::Custom事件)
    esp_err_t setCustomColor(uint32_t color);

    // 设置亮度 (0-255)
    esp_err_t setBrightness(uint8_t brightness);

    // 获取当前亮度
    uint8_t getBrightness() const;

    // 检查服务是否已初始化
    bool isInitialized() const;

    // 注册自定义事件处理器
    void registerEventHandler(std::function<void(LedEvent)> handler);

private:
    // 私有构造函数（单例模式）
    LedService();
    ~LedService();

    // 禁用拷贝构造和赋值操作符
    LedService(const LedService &) = delete;
    LedService &operator=(const LedService &) = delete;

    // LED服务任务
    static void ledServiceTask(void *arg);

    // 处理LED事件
    void handleLedEvent(LedEvent event);

    // 设置所有LED为指定颜色
    void setAllLeds(uint8_t red, uint8_t green, uint8_t blue);

    // 清除所有LED
    void clearAllLeds();

    // 闪烁效果
    void blinkEffect(uint8_t red, uint8_t green, uint8_t blue, int times, int delayMs);

    // 流水灯效果
    void chaseEffect(uint8_t red, uint8_t green, uint8_t blue);

    // 彩虹流水灯效果
    void rainbowChaseEffect();

    // 波浪效果
    void waveEffect(uint8_t red, uint8_t green, uint8_t blue);

    // 彩虹效果
    void rainbowEffect();

    // 呼吸灯效果
    void breathingEffect(uint8_t red, uint8_t green, uint8_t blue);

    // HSV转RGB转换函数
    struct RgbColor
    {
        uint8_t r, g, b;
    };
    struct HsvColor
    {
        float h, s, v; // Hue (0-360), Saturation (0-1), Value (0-1)
    };
    RgbColor hsvToRgb(HsvColor hsv);

    // 应用亮度到颜色值
    uint8_t applyBrightness(uint8_t value);

private:
    // LED灯带句柄
    led_strip_handle_t m_ledStrip;

    // 事件队列
    QueueHandle_t m_ledEventQueue;

    // 亮度设置 (0-255)
    uint8_t m_brightness;

    // 服务是否已初始化
    bool m_serviceInitialized;

    // LED配置
    LedConfig m_ledConfig;

    // 任务句柄
    TaskHandle_t m_ledTaskHandle;

    // 自定义事件处理器
    std::function<void(LedEvent)> m_eventHandler;
};