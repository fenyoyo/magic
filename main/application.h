#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <string>
class Application
{
private:
    /* data */
    Application();
    ~Application();

    bool m_mqtt_connected;
    bool m_device_status;

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