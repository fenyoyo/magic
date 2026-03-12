/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <cstdint>
#include <string>
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "public.h"

/**
 * @brief BLE Manager class for handling NimBLE stack operations
 *
 * This class encapsulates all BLE functionality including:
 * - GAP service (advertising, connection management)
 * - GATT server services
 * - Event handling
 */
class BleManager
{
public:
    /**
     * @brief Get singleton instance
     * @return Reference to BleManager instance
     */
    static BleManager &getInstance();

    /**
     * @brief Initialize BLE stack and services
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t init();

    /**
     * @brief Start advertising
     */
    void startAdvertising();

    /**
     * @brief Send heart rate indication
     */
    void sendHeartRateIndication();

    /**
     * @brief Start heart rate monitoring
     */
    void startHeartRateMonitoring();

    /**
     * @brief Stop heart rate monitoring
     */
    void stopHeartRateMonitoring();

    /**
     * @brief Set device name
     * @param name Device name to be advertised
     * @return 0 on success, error code otherwise
     */
    int setDeviceName(const char *name);

    /**
     * @brief Get current connection status
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return m_connected; }

    /**
     * @brief Get current connection handle
     * @return Connection handle or BLE_HS_CONN_HANDLE_NONE if not connected
     */
    uint16_t getConnHandle() const { return m_conn_handle; }

    /**
     * @brief Get Bluetooth MAC address
     * @return String representation of Bluetooth MAC address (XX:XX:XX:XX:XX:XX)
     */
    std::string getBluetoothMacAddress();

    /**
     * @brief Notify WiFi connection status to subscribed clients
     * @param connected True if WiFi is connected, false otherwise
     */
    void notifyWifiStatus(bool connected);

    /**
     * @brief Notify MQTT connection status to subscribed clients
     * @param connected True if MQTT is connected, false otherwise
     */
    void notifyMqttStatus(bool connected);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    BleManager();

    /**
     * @brief Private destructor
     */
    ~BleManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    BleManager(const BleManager &) = delete;
    BleManager &operator=(const BleManager &) = delete;

    /**
     * @brief Configure NimBLE host
     */
    void configureHost();

    /**
     * @brief GAP event handler
     * @param event Pointer to GAP event
     * @param arg User argument (unused)
     * @return 0 on success, error code otherwise
     */
    static int gapEventHandler(struct ble_gap_event *event, void *arg);

    /**
     * @brief Handle GAP event (internal dispatcher)
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleGapEvent(struct ble_gap_event *event);

    /**
     * @brief Handle connect event
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleConnect(struct ble_gap_event *event);

    /**
     * @brief Handle disconnect event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleDisconnect(struct ble_gap_event *event);

    /**
     * @brief Handle connection update event
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleConnUpdate(struct ble_gap_event *event);

    /**
     * @brief Handle advertise complete event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleAdvComplete(struct ble_gap_event *event);

    /**
     * @brief Handle notify TX event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleNotifyTx(struct ble_gap_event *event);

    /**
     * @brief Handle subscribe event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleSubscribe(struct ble_gap_event *event);
    static void heart_rate_task(void *param);

    /**
     * @brief Handle MTU update event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleMtu(struct ble_gap_event *event);

    /**
     * @brief Print connection descriptor
     * @param desc Pointer to connection descriptor
     */
    void printConnDesc(struct ble_gap_conn_desc *desc);

    /**
     * @brief Format MAC address to string
     * @param addr_str Output string buffer
     * @param addr MAC address bytes
     */
    static void formatAddr(char *addr_str, uint8_t addr[]);

    // Member variables
    uint8_t m_ownAddrType;        ///< Own address type for advertising
    uint8_t m_addrVal[6];         ///< Device MAC address
    bool m_connected;             ///< Connection status
    uint16_t m_conn_handle;       ///< Current connection handle
    static uint8_t m_wifi_status; ///< Current WiFi status
    static uint8_t m_mqtt_status; ///< Current MQTT status

    static int magic_learning_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                         struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int mqtt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

    static int ssid_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

    // /* Automation IO service */
    static const ble_uuid16_t auto_io_svc_uuid;
    static uint16_t ssid_chr_val_handle;
    static const ble_uuid16_t ssid_chr_uuid;
    static uint16_t password_chr_val_handle;
    static const ble_uuid16_t password_chr_uuid;
    static uint16_t connect_chr_val_handle;
    static const ble_uuid16_t connect_chr_uuid;

    static uint16_t connect_status_chr_conn_handle;
    static uint16_t connect_status_chr_val_handle;
    static const ble_uuid16_t connect_status_chr_uuid;
    static uint16_t mqtt_status_chr_val_handle;
    static const ble_uuid16_t mqtt_status_chr_uuid;
    static uint16_t mac_addr_chr_val_handle;
    static const ble_uuid16_t mac_addr_chr_uuid; // Custom UUID for MAC Address

    /* MQTT Configuration service */
    static const ble_uuid16_t mqtt_config_svc_uuid; // Custom UUID for MQTT Configuration Service
    static uint16_t mqtt_addr_chr_val_handle;
    static const ble_uuid16_t mqtt_addr_chr_uuid; // Custom UUID for MQTT Server Address
    static uint16_t mqtt_username_chr_val_handle;
    static const ble_uuid16_t mqtt_username_chr_uuid; // Custom UUID for MQTT Username
    static uint16_t mqtt_password_chr_val_handle;
    static const ble_uuid16_t mqtt_password_chr_uuid; // Custom UUID for MQTT Password
    static uint16_t mqtt_port_chr_val_handle;
    static const ble_uuid16_t mqtt_port_chr_uuid; // Custom UUID for MQTT Port

    /* GATT services table */

    static const ble_uuid16_t magic_learning_svc_uuid; // Custom UUID for Magic Learning Service
    static const ble_uuid16_t magic_enable_chr_uuid;   // Custom UUID for Magic Enable characteristic
    static uint16_t magic_enable_chr_val_handle;

    static const struct ble_gatt_svc_def gatt_svr_svcs[];
};

#endif // BLE_MANAGER_H