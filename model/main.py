import paho.mqtt.client as mqtt
import json
import csv
import os
from datetime import datetime
import  visualize_gyro_data


# command = 'circular'
# command = 'left'
# command = 'right'
# command = 'lightning'
# command = 'triangle'
# command = 'up'
command = 'down'

class GyroDataCollector:
    def __init__(self, csv_filename='gyro_data.csv'):
        self.csv_filename = csv_filename
        self.is_recording = False
        self.record_count = 0
        self.current_session_file = None
        self.fieldnames = []  # 动态字段列表

    def start_recording(self):
        """开始记录数据"""
        if not self.is_recording:
            # 生成带序号的文件名
            # 查找当前目录下已有的文件数量，用于生成下一个序号
            dataset_dir = f'dataset/{command}'
            os.makedirs(dataset_dir, exist_ok=True)
            
            # 获取当前目录下所有example_*.csv文件
            import glob
            existing_files = glob.glob(os.path.join(dataset_dir, 'example_*.csv'))
            # 提取数字序号并找到最大值
            max_num = 0
            for file in existing_files:
                try:
                    filename = os.path.basename(file)
                    num_str = filename.replace('example_', '').replace('.csv', '')
                    if num_str.isdigit():
                        num = int(num_str)
                        if num > max_num:
                            max_num = num
                except:
                    continue
            
            # 使用下一个序号
            next_num = max_num + 1
            self.csv_filename = f'{dataset_dir}/example_{next_num:03d}.csv'

            # 初始化新的CSV文件，但不写入表头直到第一次数据到达
            # 准备目录
            os.makedirs(os.path.dirname(self.csv_filename), exist_ok=True)

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
            # acceleration_visualizer.show(self.csv_filename)

    def save_data(self, data):
        """保存单条数据"""
        if not self.is_recording:
            return False

        try:
            # 如果是第一条数据，初始化CSV文件并写入表头
            file_exists = os.path.exists(self.csv_filename)
            with open(self.csv_filename, 'a', newline='', encoding='utf-8') as csvfile:
                # 获取数据的所有键作为字段名
                if not file_exists:
                    # 第一次写入，写入表头
                    self.fieldnames = list(data.keys())
                    writer = csv.DictWriter(csvfile, fieldnames=self.fieldnames)
                    writer.writeheader()
                else:
                    # 检查是否需要添加新字段
                    new_fields = [key for key in data.keys() if key not in self.fieldnames]
                    if new_fields:
                        # 需要扩展CSV文件结构，这比较复杂，我们保持原有字段不变
                        # 或者我们可以重新组织现有CSV文件以包含新字段
                        print(f"发现新字段: {new_fields}，将使用现有字段结构继续记录")
                    
                    # 使用现有的字段名进行写入
                    writer = csv.DictWriter(csvfile, fieldnames=self.fieldnames)
                
                # 只写入存在于当前fieldnames中的字段值
                filtered_data = {k: v for k, v in data.items() if k in self.fieldnames}
                writer.writerow(filtered_data)
                
            self.record_count += 1
            seq_value = data.get('seq', data.get('sequence', 'N/A'))
            print(f"✓ 记录数据 #{self.record_count} (seq: {seq_value})")
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

            # 处理不同格式的数据：如果接收的是"time"字段，则将其转换为"dt"
            if 'time' in payload and 'dt' not in payload:
                payload['dt'] = payload['time']

            # 显示消息内容 - 显示所有字段而不是固定的几个
            print(f"\n[{timestamp}] 收到陀螺仪数据:")
            fields_info = ", ".join([f"{k}={v}" for k, v in payload.items()])
            print(f"  数据: {fields_info}")

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