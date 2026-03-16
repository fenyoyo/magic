#include "mqtt_manager.h"
#include "esp_log.h"
#include "application.h"
#include "public.h"
#include "NVSManager.h"
#include <string.h>

const char *MQTTManager::TAG = "MQTTManager";
MQTTManager *MQTTManager::s_instance = nullptr;

MQTTManager::MQTTManager()
    : m_client(nullptr), m_is_connected(false), m_port(1883) // 默认MQTT端口
{
}

MQTTManager::~MQTTManager()
{
    stop();
    s_instance = nullptr;
}

MQTTManager &MQTTManager::getInstance()
{
    if (s_instance == nullptr)
    {
        s_instance = new MQTTManager();
    }
    return *s_instance;
}

void MQTTManager::handleConnected(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    m_is_connected = true;
    auto &app = Application::getInstance();
    xEventGroupSetBits(app.event_group, MQTT_CONNECTED_BIT);

    subscribe(app.mac_address + "/events/predict/result", 1);
}

void MQTTManager::handleDisconnected()
{
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    m_is_connected = false;
}

void MQTTManager::handleData(esp_mqtt_event_handle_t event)
{
    std::string topic_str(event->topic, event->topic_len);
    auto &app = Application::getInstance();
    std::string topic = app.mac_address + "/events/predict/result";
    if (topic_str == topic)
    {
        ESP_LOGI(TAG, "Received inference result on topic: %s", topic.c_str());
        std::string data_str(event->data, event->data_len);
        ESP_LOGI(TAG, "Data: %s", data_str.c_str());
        LedService::getInstance().triggerEvent(LedEvent::GameStart);
    }

    // 触发消息回调
    // if (m_message_callback)
    // {
    //     std::string topic(event->topic, event->topic_len);
    //     std::string data(event->data, event->data_len);
    //     m_message_callback(topic, data, event->data_len);
    // }
}

void MQTTManager::handleError(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

    if (event->error_handle)
    {
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x",
                     event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x",
                     event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",
                     event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x",
                     event->error_handle->connect_return_code);
        }
    }

    // 触发错误回调
    // if (m_error_callback)
    // {
    //     m_error_callback(event->error_handle ? event->error_handle->error_type : -1,
    //                      event->error_handle);
    // }
}

void MQTTManager::handleSubscribed(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
}

void MQTTManager::handleUnsubscribed(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
}

void MQTTManager::handlePublished(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
}

void MQTTManager::eventHandler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    MQTTManager &instance = MQTTManager::getInstance();
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // 连接事件
        instance.handleConnected(event);
        break;

    case MQTT_EVENT_DISCONNECTED:
        instance.handleDisconnected();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        instance.handleSubscribed(event);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        instance.handleUnsubscribed(event);
        break;

    case MQTT_EVENT_PUBLISHED:
        instance.handlePublished(event);
        break;

    case MQTT_EVENT_DATA:
        // 消息事件
        instance.handleData(event);
        break;

    case MQTT_EVENT_ERROR:
        instance.handleError(event);
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

esp_err_t MQTTManager::init()
{
    if (m_client != nullptr)
    {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return ESP_OK;
    }

    // 从NVS中重新读取最新的配置信息
    NVSManager nvsManager("storage");
    nvsManager.init();

    std::string broker_uri = nvsManager.readString(MQTT_ADDR);
    std::string username = nvsManager.readString(MQTT_USERNAME);
    std::string password = nvsManager.readString(MQTT_PASSWORD);

    // 读取端口配置，如果未设置则使用默认值
    int32_t port_value = 0;
    if (nvsManager.readInt(MQTT_PORT, &port_value))
    {
        m_port = static_cast<int>(port_value);
    }
    else
    {
        m_port = 1883; // 默认MQTT端口
    }

    // 检查必要配置是否存在
    if (broker_uri.empty())
    {
        ESP_LOGW(TAG, "MQTT broker URI is not configured in NVS");
        return ESP_ERR_INVALID_ARG;
    }

    if (username.empty())
    {
        ESP_LOGE(TAG, "MQTT username is not configured in NVS");
        return ESP_ERR_INVALID_ARG;
    }

    if (password.empty())
    {
        ESP_LOGE(TAG, "MQTT password is not configured in NVS");
        return ESP_ERR_INVALID_ARG;
    }

    // 配置MQTT客户端
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri.c_str();

    if (!username.empty())
    {
        mqtt_cfg.credentials.username = username.c_str();
    }

    if (!password.empty())
    {
        mqtt_cfg.credentials.authentication.password = password.c_str();
    }

    // 初始化客户端
    m_client = esp_mqtt_client_init(&mqtt_cfg);

    if (m_client == nullptr)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    // 注册事件处理器
    esp_mqtt_client_register_event(m_client,
                                   (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   eventHandler,
                                   nullptr);

    // 启动MQTT客户端
    esp_err_t ret = esp_mqtt_client_start(m_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started successfully");
    return ESP_OK;
}

esp_err_t MQTTManager::stop()
{
    if (m_client == nullptr)
    {
        ESP_LOGW(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mqtt_client_stop(m_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_mqtt_client_destroy(m_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to destroy MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    m_client = nullptr;
    m_is_connected = false;
    ESP_LOGI(TAG, "MQTT client stopped");

    return ESP_OK;
}

int MQTTManager::publish(const std::string &topic,
                         const std::string &data,
                         int qos,
                         int retain)
{
    return publish(topic, data.c_str(), data.length(), qos, retain);
}

int MQTTManager::publish(const std::string &topic,
                         const void *data,
                         size_t len,
                         int qos,
                         int retain)
{
    if (m_client == nullptr)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (!m_is_connected)
    {
        ESP_LOGW(TAG, "MQTT client not connected, message may be queued");
    }

    int msg_id = esp_mqtt_client_publish(m_client,
                                         topic.c_str(),
                                         (const char *)data,
                                         len,
                                         qos,
                                         retain);

    if (msg_id < 0)
        ESP_LOGE(TAG, "Failed to publish message to topic: %s (queue full?)", topic.c_str());

    return msg_id;
}

int MQTTManager::subscribe(const std::string &topic, int qos)
{
    if (m_client == nullptr)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (!m_is_connected)
    {
        ESP_LOGW(TAG, "MQTT client not connected");
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(m_client, topic.c_str(), qos);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic.c_str());
    }
    else
    {
        ESP_LOGI(TAG, "Subscribed to topic: %s, msg_id=%d", topic.c_str(), msg_id);
    }

    return msg_id;
}

int MQTTManager::unsubscribe(const std::string &topic)
{
    if (m_client == nullptr)
    {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (!m_is_connected)
    {
        ESP_LOGW(TAG, "MQTT client not connected");
        return -1;
    }

    int msg_id = esp_mqtt_client_unsubscribe(m_client, topic.c_str());
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic.c_str());
    }
    else
    {
        ESP_LOGI(TAG, "Unsubscribed from topic: %s, msg_id=%d", topic.c_str(), msg_id);
    }

    return msg_id;
}