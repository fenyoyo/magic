/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <cstdint>
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"

/**
 * @brief BLE Manager class for handling NimBLE stack operations
 * 
 * This class encapsulates all BLE functionality including:
 * - GAP service (advertising, connection management)
 * - GATT server services
 * - Event handling
 */
class BleManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to BleManager instance
     */
    static BleManager& getInstance();

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
     * @brief Set device name
     * @param name Device name to be advertised
     * @return 0 on success, error code otherwise
     */
    int setDeviceName(const char* name);

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
    BleManager(const BleManager&) = delete;
    BleManager& operator=(const BleManager&) = delete;

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
    static int gapEventHandler(struct ble_gap_event* event, void* arg);

    /**
     * @brief Handle GAP event (internal dispatcher)
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleGapEvent(struct ble_gap_event* event);

    /**
     * @brief Handle connect event
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleConnect(struct ble_gap_event* event);

    /**
     * @brief Handle disconnect event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleDisconnect(struct ble_gap_event* event);

    /**
     * @brief Handle connection update event
     * @param event Pointer to GAP event
     * @return 0 on success, error code otherwise
     */
    int handleConnUpdate(struct ble_gap_event* event);

    /**
     * @brief Handle advertise complete event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleAdvComplete(struct ble_gap_event* event);

    /**
     * @brief Handle notify TX event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleNotifyTx(struct ble_gap_event* event);

    /**
     * @brief Handle subscribe event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleSubscribe(struct ble_gap_event* event);

    /**
     * @brief Handle MTU update event
     * @param event Pointer to GAP event
     * @return 0 on success
     */
    int handleMtu(struct ble_gap_event* event);

    /**
     * @brief Print connection descriptor
     * @param desc Pointer to connection descriptor
     */
    void printConnDesc(struct ble_gap_conn_desc* desc);

    /**
     * @brief Format MAC address to string
     * @param addr_str Output string buffer
     * @param addr MAC address bytes
     */
    static void formatAddr(char* addr_str, uint8_t addr[]);

    // Member variables
    uint8_t m_ownAddrType;          ///< Own address type for advertising
    uint8_t m_addrVal[6];           ///< Device MAC address
    bool m_connected;               ///< Connection status
    uint16_t m_conn_handle;         ///< Current connection handle
};

#endif // BLE_MANAGER_H