# ESP32 WiFi + MQTT 项目

这是一个基于ESP-IDF的ESP32项目，实现了WiFi连接和MQTT客户端功能。

## 功能特性

- ✅ WiFi Station模式连接
- ✅ MQTT客户端连接
- ✅ MQTT消息订阅和发布
- ✅ 完整的事件处理机制
- ✅ 可配置的WiFi和MQTT参数

## 项目结构

```
gamic/
├── CMakeLists.txt              # 项目主配置文件
├── main/
│   ├── CMakeLists.txt          # 主组件配置（包含mqtt依赖）
│   ├── Kconfig.projbuild       # 配置菜单定义
│   └── main.cc                 # 主程序代码
└── README.md                   # 本文件
```

## 编译和烧录

### 1. 配置ESP-IDF环境

在Windows上，打开ESP-IDF命令提示符，或在PowerShell中运行：

```powershell
# 导航到ESP-IDF安装目录并激活环境
. $env:IDF_PATH/export.ps1
```

或在CMD中：

```cmd
%IDF_PATH%\export.bat
```

### 2. 配置项目

```bash
cd e:\Projects\gamic
idf.py menuconfig
```

在配置菜单中设置：

#### WiFi配置 (Example Configuration)
- **WiFi SSID**: 你的WiFi名称
- **WiFi Password**: 你的WiFi密码
- **Maximum retry**: WiFi重连次数（默认5次）

#### MQTT配置 (MQTT Configuration)
- **MQTT Broker URI**: MQTT服务器地址
  - 示例: `mqtt://broker.emqx.io:1883`
  - 支持: mqtt://, mqtts://, ws://, wss://
- **MQTT Username**: MQTT用户名（可选）
- **MQTT Password**: MQTT密码（可选）
- **MQTT Subscribe Topic**: 订阅的主题（默认: `/topic/test/subscribe`）
- **MQTT Publish Topic**: 发布的主题（默认: `/topic/test/publish`）

### 3. 编译项目

```bash
idf.py build
```

### 4. 烧录到设备

```bash
idf.py -p COM3 flash
```

> 注意：将 `COM3` 替换为你的ESP32实际连接的串口号

### 5. 查看日志

```bash
idf.py -p COM3 monitor
```

或者一次性完成烧录和监控：

```bash
idf.py -p COM3 flash monitor
```

## 代码说明

### MQTT功能实现

#### 1. MQTT事件处理

代码实现了完整的MQTT事件处理器，包括：

- **MQTT_EVENT_CONNECTED**: 连接成功后自动订阅主题并发布测试消息
- **MQTT_EVENT_DISCONNECTED**: 断开连接事件
- **MQTT_EVENT_SUBSCRIBED**: 订阅成功确认
- **MQTT_EVENT_PUBLISHED**: 发布成功确认
- **MQTT_EVENT_DATA**: 接收到订阅主题的消息
- **MQTT_EVENT_ERROR**: 错误处理（包括TCP传输错误、连接拒绝等）

#### 2. 连接流程

```
启动 → 初始化NVS → WiFi连接 → 获取IP → 启动MQTT客户端 → 连接MQTT服务器
```

#### 3. 主要函数

- `wifi_init_sta()`: 初始化并连接WiFi
- `mqtt_event_handler()`: 处理MQTT事件
- `mqtt_app_start()`: 初始化并启动MQTT客户端

### 自定义MQTT行为

在 `main.cc` 的 `mqtt_event_handler()` 函数中，你可以修改：

```cpp
case MQTT_EVENT_CONNECTED:
    // 连接成功后的操作
    // 修改订阅的主题
    esp_mqtt_client_subscribe(client, "your/custom/topic", 0);
    
    // 修改发布的消息
    esp_mqtt_client_publish(client, "your/topic", "your message", 0, 1, 0);
    break;

case MQTT_EVENT_DATA:
    // 处理接收到的消息
    ESP_LOGI(MQTT_TAG, "收到消息: %.*s", event->data_len, event->data);
    // 在这里添加你的业务逻辑
    break;
```

## 常用MQTT测试服务器

- **Eclipse IoT**: `mqtt://mqtt.eclipseprojects.io:1883`
- **EMQX公共服务器**: `mqtt://broker.emqx.io:1883`
- **HiveMQ公共服务器**: `mqtt://broker.hivemq.com:1883`
- **Mosquitto测试服务器**: `mqtt://test.mosquitto.org:1883`

## 调试技巧

### 查看详细日志

在 `menuconfig` 中设置日志级别：
```
Component config → Log output → Default log verbosity → Debug
```

### 测试MQTT连接

使用MQTT客户端工具测试：

1. **MQTTX** (图形界面): https://mqttx.app/
2. **mosquitto_sub** (命令行):
   ```bash
   mosquitto_sub -h broker.emqx.io -t "/topic/test/publish"
   ```
3. **mosquitto_pub** (命令行):
   ```bash
   mosquitto_pub -h broker.emqx.io -t "/topic/test/subscribe" -m "Hello ESP32"
   ```

## 故障排除

### 1. WiFi连接失败
- 检查SSID和密码是否正确
- 确认WiFi信号强度
- 检查WiFi加密方式是否支持

### 2. MQTT连接失败
- 确认MQTT服务器地址和端口正确
- 检查网络连接是否正常
- 验证用户名和密码（如果需要）
- 查看日志中的错误信息

### 3. 编译错误
- 确保ESP-IDF环境已正确安装和激活
- 检查ESP-IDF版本（建议使用v4.4或更高版本）
- 清理构建目录: `idf.py fullclean`

## 依赖组件

- `esp_wifi`: WiFi驱动
- `nvs_flash`: 非易失性存储
- `freertos`: 实时操作系统
- `mqtt`: MQTT客户端库

## 许可证

本项目代码基于ESP-IDF示例，采用Public Domain或CC0许可。

## 参考资料

- [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [ESP-MQTT文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/mqtt.html)
- [MQTT协议规范](https://mqtt.org/)
