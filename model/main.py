import paho.mqtt.client as mqtt
import json
import csv
import os
from datetime import datetime
import  visualize_gyro_data


command = 'triangle'

class GyroDataCollector:
    def __init__(self, csv_filename='gyro_data.csv'):
        self.csv_filename = csv_filename
        self.is_recording = False
        self.record_count = 0
        self.current_session_file = None

    def start_recording(self):
        """开始记录数据"""
        if not self.is_recording:
            # 生成带时间戳的文件名
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            self.csv_filename = f'{command}/gyro_data_{timestamp}.csv'

            # 初始化新的CSV文件
            with open(self.csv_filename, 'w', newline='', encoding='utf-8') as csvfile:
                fieldnames = ['timestamp', 'seq', 'gx', 'gy', 'gz', 'ax', 'ay', 'az']
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                writer.writeheader()

            self.is_recording = True
            self.record_count = 0
            print(f"\n=== 开始记录数据 ===")
            print(f"保存到文件: {self.csv_filename}")
            print("=" * 30)

    def stop_recording(self):
        """停止记录数据"""
        if self.is_recording:
            self.is_recording = False
            print(f"\n=== 停止记录数据 ===")
            print(f"本次记录数据条数: {self.record_count}")
            print(f"保存文件: {self.csv_filename}")
            print("=" * 30)
            visualize_gyro_data.visualize_gyro_data(self.csv_filename)

    def save_data(self, data):
        """保存单条数据"""
        if not self.is_recording:
            return False

        try:
            # 添加时间戳
            data_with_time = data.copy()
            data_with_time['timestamp'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]

            # 写入CSV文件
            with open(self.csv_filename, 'a', newline='', encoding='utf-8') as csvfile:
                fieldnames = ['timestamp', 'seq', 'gx', 'gy', 'gz', 'ax', 'ay', 'az']
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                writer.writerow(data_with_time)
            self.record_count += 1
            print(f"✓ 记录数据 #{self.record_count} (seq: {data.get('seq', 'N/A')})")
            return True

        except Exception as e:
            print(f"保存数据失败: {e}")
            return False


# 创建数据收集器实例
collector = GyroDataCollector()


# 回调函数：当客户端收到服务器的连接响应时调用
def on_connect(client, userdata, flags, rc, properties):
    if rc == 0:
        print("成功连接到MQTT服务器")
        # 订阅所有需要的主题
        client.subscribe([("/device/gyro", 0), ("/device/start", 0), ("/device/stop", 0)])
        print("已订阅主题: /device/gyro, /device/start, /device/stop")
        print("等待开始命令...")
        print("-" * 40)
    else:
        print(f"连接失败，返回码: {rc}")


# 回调函数：当收到消息时调用
def on_message(client, userdata, msg):
    topic = msg.topic
    timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]

    try:
        # 处理控制消息
        if topic == "/device/start":
            print(f"\n[{timestamp}] 收到开始命令")
            collector.start_recording()
            return

        elif topic == "/device/stop":
            print(f"\n[{timestamp}] 收到停止命令")
            collector.stop_recording()
            return

        # 处理陀螺仪数据
        elif topic == "/device/gyro":

            # 解析JSON格式的消息
            payload = json.loads(msg.payload.decode('utf-8'))
            # 显示消息内容
            print(f"\n[{timestamp}] 收到陀螺仪数据:")
            print(f"  数据: seq={payload.get('seq')}, "
                  f"gx={payload.get('gx'):.2f}, gy={payload.get('gy'):.2f}, gz={payload.get('gz'):.2f}, "
                  f"ax={payload.get('ax'):.2f}, ay={payload.get('ay'):.2f}, az={payload.get('az'):.2f}")

            # 只有在记录状态下才保存数据
            if collector.is_recording:
                collector.save_data(payload)
                print(f"  状态: [记录中] 已保存到 {collector.csv_filename}")
            else:
                print(f"  状态: [等待开始] 数据未保存")

        else:
            print(f"收到未知主题消息: {topic}")

    except json.JSONDecodeError:
        print(f"收到非JSON消息 - 主题: {topic}, 内容: {msg.payload.decode('utf-8')}")
    except Exception as e:
        print(f"处理消息时出错: {e}")


# 创建MQTT客户端
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

# 绑定回调函数
client.on_connect = on_connect
client.on_message = on_message

# 连接MQTT服务器
broker_address = "mqtt.uyuo.me"

try:
    print("=== 陀螺仪数据收集器 ===")
    print("控制命令:")
    print("  - 发送 /device/start 开始记录")
    print("  - 发送 /device/stop  停止记录")
    print("  - 按 Ctrl+C 退出程序")
    print("=" * 40)

    print(f"正在连接服务器 {broker_address}...")
    client.connect(broker_address, port=1883, keepalive=60)

    # 启动网络循环
    client.loop_forever()

except KeyboardInterrupt:
    print("\n\n正在停止程序...")

    # 如果正在记录，先停止记录
    if collector.is_recording:
        collector.stop_recording()

    client.disconnect()
    print("程序已退出")

except Exception as e:
    print(f"错误: {e}")