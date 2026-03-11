#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string>

#define TAG "NVSManager"
class NVSManager
{
private:
    nvs_handle_t nvs_handle;
    const char *namespace_name;
    bool initialized;

public:
    NVSManager(const char *namespace_name = "storage")
        : namespace_name(namespace_name), initialized(false) {}

    ~NVSManager()
    {
        if (initialized)
        {
            nvs_close(nvs_handle);
        }
    }

    bool init();

    // 写入整型
    bool writeInt(const char *key, int32_t value);

    // 读取整型
    bool readInt(const char *key, int32_t *value);

    // 写入字符串
    bool writeString(const char *key, const char *value);

    // 读取字符串
    std::string readString(const char *key);

    // 写入二进制数据
    bool writeBlob(const char *key, const void *data, size_t length)
    {
        if (!initialized)
            return false;

        esp_err_t err = nvs_set_blob(nvs_handle, key, data, length);
        if (err != ESP_OK)
            return false;

        err = nvs_commit(nvs_handle);
        return err == ESP_OK;
    }

    // 读取二进制数据
    bool readBlob(const char *key, void *data, size_t *length)
    {
        if (!initialized)
            return false;
        return nvs_get_blob(nvs_handle, key, data, length) == ESP_OK;
    }

    // 删除键
    bool eraseKey(const char *key)
    {
        if (!initialized)
            return false;

        esp_err_t err = nvs_erase_key(nvs_handle, key);
        if (err != ESP_OK)
            return false;

        err = nvs_commit(nvs_handle);
        return err == ESP_OK;
    }

    // 清空命名空间
    bool eraseAll()
    {
        if (!initialized)
            return false;

        esp_err_t err = nvs_erase_all(nvs_handle);
        if (err != ESP_OK)
            return false;

        err = nvs_commit(nvs_handle);
        return err == ESP_OK;
    }
};
#endif