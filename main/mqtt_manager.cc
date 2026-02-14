#include "mqtt_manager.h"
#include "esp_log.h"
#include <string.h>

const char *MQTTManager::TAG = "MQTTManager";
MQTTManager *MQTTManager::s_instance = nullptr;

MQTTManager::MQTTManager()
    : m_client(nullptr), m_is_connected(false), m_broker_uri(CONFIG_MQTT_BROKER_URI),
      m_subscribe_topic(CONFIG_MQTT_SUBSCRIBE_TOPIC),
      m_publish_topic(CONFIG_MQTT_PUBLISH_TOPIC)
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

void MQTTManager::setMessageCallback(MessageCallback callback)
{
    m_message_callback = callback;
}

void MQTTManager::setConnectionCallback(ConnectionCallback callback)
{
    m_connection_callback = callback;
}

void MQTTManager::setErrorCallback(ErrorCallback callback)
{
    m_error_callback = callback;
}

void MQTTManager::handleConnected(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    m_is_connected = true;

    // 触发连接回调
    if (m_connection_callback)
    {
        m_connection_callback(true);
    }

    // 自动订阅配置的主题
    if (!m_subscribe_topic.empty())
    {
        int msg_id = esp_mqtt_client_subscribe(m_client, m_subscribe_topic.c_str(), 0);
        ESP_LOGI(TAG, "Sent subscribe successful, msg_id=%d", msg_id);
    }

    // 发布连接成功消息
    if (!m_publish_topic.empty())
    {
        int msg_id = esp_mqtt_client_publish(m_client,
                                             m_publish_topic.c_str(),
                                             "ESP32 Connected",
                                             0, 1, 0);
        ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
    }
}

void MQTTManager::handleDisconnected()
{
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    m_is_connected = false;

    // 触发连接回调
    if (m_connection_callback)
    {
        m_connection_callback(false);
    }
}

void MQTTManager::handleData(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
    ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

    // 触发消息回调
    if (m_message_callback)
    {
        std::string topic(event->topic, event->topic_len);
        std::string data(event->data, event->data_len);
        m_message_callback(topic, data, event->data_len);
    }
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
    if (m_error_callback)
    {
        m_error_callback(event->error_handle ? event->error_handle->error_type : -1,
                         event->error_handle);
    }
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

    // 配置MQTT客户端
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = m_broker_uri.c_str();

    if (!m_username.empty())
    {
        mqtt_cfg.credentials.username = m_username.c_str();
    }

    if (!m_password.empty())
    {
        mqtt_cfg.credentials.authentication.password = m_password.c_str();
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