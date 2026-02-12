#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <functional>
#include <string>
#include "mqtt_client.h"
#include "esp_event.h"

class MQTTManager
{
public:
    // 消息回调类型定义
    using MessageCallback = std::function<void(const std::string &topic,
                                               const std::string &data,
                                               int data_len)>;

    // 连接状态回调类型定义
    using ConnectionCallback = std::function<void(bool connected)>;

    // 错误回调类型定义
    using ErrorCallback = std::function<void(int error_type, void *error_data)>;

    // 获取单例实例
    static MQTTManager &getInstance();

    // 禁止拷贝和赋值
    MQTTManager(const MQTTManager &) = delete;
    MQTTManager &operator=(const MQTTManager &) = delete;

    // 初始化MQTT客户端
    esp_err_t init();

    // 停止MQTT客户端
    esp_err_t stop();

    // 发布消息
    int publish(const std::string &topic,
                const std::string &data,
                int qos = 1,
                int retain = 0);

    // 发布二进制数据
    int publish(const std::string &topic,
                const void *data,
                size_t len,
                int qos = 1,
                int retain = 0);

    // 订阅主题
    int subscribe(const std::string &topic, int qos = 0);

    // 取消订阅
    int unsubscribe(const std::string &topic);

    // 检查连接状态
    bool isConnected() const { return m_is_connected; }

    // 获取MQTT客户端句柄
    esp_mqtt_client_handle_t getClient() const { return m_client; }

    // 设置回调函数
    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // 设置成员函数回调（模板方法）
    template <typename T>
    void setMessageCallback(T *instance, void (T::*callback)(const std::string &, const std::string &, int))
    {
        m_message_callback = [instance, callback](const std::string &topic,
                                                  const std::string &data,
                                                  int data_len)
        {
            (instance->*callback)(topic, data, data_len);
        };
    }

    template <typename T>
    void setConnectionCallback(T *instance, void (T::*callback)(bool))
    {
        m_connection_callback = [instance, callback](bool connected)
        {
            (instance->*callback)(connected);
        };
    }

    template <typename T>
    void setErrorCallback(T *instance, void (T::*callback)(int, void *))
    {
        m_error_callback = [instance, callback](int error_type, void *error_data)
        {
            (instance->*callback)(error_type, error_data);
        };
    }

private:
    MQTTManager();
    ~MQTTManager();

    // 事件处理函数
    static void eventHandler(void *handler_args,
                             esp_event_base_t base,
                             int32_t event_id,
                             void *event_data);

    // 内部辅助函数
    void handleConnected(esp_mqtt_event_handle_t event);
    void handleDisconnected();
    void handleData(esp_mqtt_event_handle_t event);
    void handleError(esp_mqtt_event_handle_t event);
    void handleSubscribed(esp_mqtt_event_handle_t event);
    void handleUnsubscribed(esp_mqtt_event_handle_t event);
    void handlePublished(esp_mqtt_event_handle_t event);

private:
    static const char *TAG;

    esp_mqtt_client_handle_t m_client;
    bool m_is_connected;

    // 回调函数
    MessageCallback m_message_callback;
    ConnectionCallback m_connection_callback;
    ErrorCallback m_error_callback;

    // 配置参数
    std::string m_broker_uri;
    std::string m_username;
    std::string m_password;
    std::string m_subscribe_topic;
    std::string m_publish_topic;

    // 静态实例指针
    static MQTTManager *s_instance;
};

#endif // MQTT_MANAGER_H