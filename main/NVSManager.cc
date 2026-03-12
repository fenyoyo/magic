#include "NVSManager.h"

#define TAG "NVSManager"
bool NVSManager::init()

{
    // 1. 初始化 NVS flash（整个系统只需要调用一次）
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS 分区需要擦除重试
        ESP_LOGW(TAG, "NVS partition needs erase, retrying...");
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS partition: %s", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init NVS: %s", esp_err_to_name(err));
        return false;
    }

    // 2. 打开命名空间
    err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open namespace '%s': %s",
                 namespace_name, esp_err_to_name(err));
        return false;
    }

    initialized = true;
    return true;
}

bool NVSManager::writeInt(const char *key, int32_t value)

{
    if (!initialized)
        return false;

    esp_err_t err = nvs_set_i32(nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write int: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(nvs_handle);
    return err == ESP_OK;
}

bool NVSManager::readInt(const char *key, int32_t *value)

{
    if (!initialized)
        return false;

    esp_err_t err = nvs_get_i32(nvs_handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key '%s' not found", key);
        return false;
    }
    return err == ESP_OK;
}

bool NVSManager::writeString(const char *key, const char *value)
{
    if (!initialized)
        return false;

    esp_err_t err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK)
        return false;

    err = nvs_commit(nvs_handle);
    return err == ESP_OK;
}

std::string NVSManager::readString(const char *key)
{
    if (!initialized)
        return "11";

    size_t required_size;
    esp_err_t err = nvs_get_str(nvs_handle, key, nullptr, &required_size);
    if (err != ESP_OK)
        return "22";

    char *buffer = new char[required_size];
    err = nvs_get_str(nvs_handle, key, buffer, &required_size);

    std::string result = (err == ESP_OK) ? buffer : "33";
    delete[] buffer;

    return result;
}