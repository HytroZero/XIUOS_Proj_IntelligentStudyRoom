import json
import logging
import threading
import time
import socket
import re
from typing import Callable, Dict, Any, List, Optional
import paho.mqtt.client as mqtt

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('iot_server')

# ------------------------------ Yeelight 设备控制相关类 ------------------------------
class YeelightDiscoverer:
    """Yeelight设备发现器（基于UDP多播）"""
    MULTICAST_ADDR = "239.255.255.250"  # 协议规定的多播地址
    MULTICAST_PORT = 1982               # 协议规定的多播端口
    SEARCH_MSG = (
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1982\r\n"
        'MAN: "ssdp:discover"\r\n'
        "ST: wifi_bulb\r\n\r\n"  # 必须以空行结尾
    ).encode("utf-8")

    @staticmethod
    def discover(timeout: int = 5) -> List[Dict]:
        """发现局域网内的Yeelight设备"""
        devices = []
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as udp_socket:
            udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            udp_socket.bind(("", YeelightDiscoverer.MULTICAST_PORT))
            udp_socket.settimeout(timeout)

            udp_socket.sendto(
                YeelightDiscoverer.SEARCH_MSG,
                (YeelightDiscoverer.MULTICAST_ADDR, YeelightDiscoverer.MULTICAST_PORT)
            )
            logger.info(f"已发送设备发现请求，等待{timeout}秒...")

            while True:
                try:
                    data, _ = udp_socket.recvfrom(1024)
                    response = data.decode("utf-8", errors="ignore")
                    device_info = YeelightDiscoverer._parse_response(response)
                    if device_info and device_info not in devices:
                        devices.append(device_info)
                except socket.timeout:
                    logger.info("设备发现超时")
                    break
                except Exception as e:
                    logger.error(f"解析设备响应失败: {str(e)}")
        return devices

    @staticmethod
    def _parse_response(response: str) -> Optional[Dict]:
        """解析灯泡的UDP响应"""
        location_match = re.search(r"Location: yeelight://([\d.]+):(\d+)", response, re.IGNORECASE)
        if not location_match:
            return None

        id_match = re.search(r"id: (\S+)", response, re.IGNORECASE)
        model_match = re.search(r"model: (\S+)", response, re.IGNORECASE)
        fw_ver_match = re.search(r"fw_ver: (\S+)", response, re.IGNORECASE)
        support_match = re.search(r"support: ([\S\s]+?)\r\n", response, re.IGNORECASE)

        return {
            "ip": location_match.group(1),
            "port": int(location_match.group(2)),
            "device_id": id_match.group(1) if id_match else "",
            "model": model_match.group(1) if model_match else "",
            "fw_version": fw_ver_match.group(1) if fw_ver_match else "",
            "supported_methods": support_match.group(1).split() if support_match else []
        }


class YeelightClient:
    """Yeelight灯泡TCP客户端"""
    def __init__(self, device_ip: str, device_port: int = 55443):
        self.device_ip = device_ip
        self.device_port = device_port
        self.tcp_socket: Optional[socket.socket] = None
        self.is_connected = False
        self.msg_id = 1  # 命令消息ID（自增）
        self.response_dict = {}  # 存储响应：{msg_id: 响应内容}
        self.response_lock = threading.Lock()
        self.notification_callback: Optional[Callable[[Dict], None]] = None  # 状态通知回调
        self.receive_thread: Optional[threading.Thread] = None

    def connect(self, notification_callback: Optional[Callable[[Dict], None]] = None) -> bool:
        """建立TCP连接并启动接收线程"""
        try:
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.settimeout(10)
            self.tcp_socket.connect((self.device_ip, self.device_port))
            self.is_connected = True
            logger.info(f"已连接到Yeelight灯泡：{self.device_ip}:{self.device_port}")

            self.notification_callback = notification_callback
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()
            return True
        except Exception as e:
            logger.error(f"TCP连接失败: {str(e)}")
            self.close()
            return False

    def _receive_loop(self):
        """设置非阻塞模式接收"""
        buffer = b""
        
        # 设置非阻塞模式
        if self.tcp_socket:
            self.tcp_socket.setblocking(False)
        
        while self.is_connected and self.tcp_socket:
            try:
                # 非阻塞接收
                try:
                    data = self.tcp_socket.recv(1024)
                    if data:
                        buffer += data
                        logger.debug(f"接收到 {len(data)} 字节数据")
                    else:
                        # 对端关闭连接
                        logger.warning("TCP连接已断开")
                        self.close()
                        break
                        
                except BlockingIOError:
                    # 没有数据可读，正常现象
                    pass
                except Exception as e:
                    logger.error(f"接收数据异常: {str(e)}")
                    self.close()
                    break
                
                # 处理缓冲区中的完整消息
                while b"\r\n" in buffer:
                    msg_bytes, buffer = buffer.split(b"\r\n", 1)
                    if msg_bytes:
                        try:
                            msg = json.loads(msg_bytes.decode("utf-8"))
                            self._process_message(msg)
                        except json.JSONDecodeError:
                            logger.error(f"解析消息失败: {msg_bytes}")
                
                # 短暂休眠，避免CPU占用过高
                time.sleep(0.1)
                
            except Exception as e:
                logger.error(f"接收循环异常: {str(e)}", exc_info=True)
                self.close()
                break

    def _process_message(self, msg: Dict):
        """处理接收到的消息"""
        if "id" in msg:
            # 命令响应
            msg_id = msg["id"]
            with self.response_lock:
                self.response_dict[msg_id] = msg
                logger.info(f"收到响应（ID: {msg_id}）: {msg}")
        elif "method" in msg and msg["method"] == "props":
            # 状态通知
            logger.info(f"收到状态通知: {msg}")
            if self.notification_callback:
                self.notification_callback(msg["params"])

    def send_command(self, method: str, params: List) -> Optional[Dict]:
        """发送控制命令到灯泡"""
        if not self.is_connected or not self.tcp_socket:
            logger.error("TCP连接未建立，无法发送命令")
            return None

        with self.response_lock:
            current_id = self.msg_id
            self.msg_id += 1
            if current_id in self.response_dict:
                del self.response_dict[current_id]

        command = {
            "id": current_id,
            "method": method,
            "params": params
        }
        command_str = json.dumps(command) + "\r\n"
        logger.info(f"发送命令（ID: {current_id}）: {command_str.strip()}")

        try:
            self.tcp_socket.sendall(command_str.encode("utf-8"))

            start_time = time.time()
            while time.time() - start_time < 5:
                with self.response_lock:
                    if current_id in self.response_dict:
                        return self.response_dict.pop(current_id)
                time.sleep(0.1)
            logger.warning(f"命令（ID: {current_id}）超时未收到响应")
            return None
        except Exception as e:
            logger.error(f"发送命令失败: {str(e)}")
            self.close()
            return None

    # 常用控制函数
    def set_power(self, power: str, effect: str = "smooth", duration: int = 500, mode: int = 0) -> Optional[Dict]:
        """控制灯泡开关"""
        if power not in ["on", "off"]:
            logger.error("power参数必须为'on'或'off'")
            return None
        return self.send_command("set_power", [power, effect, duration, mode])

    def set_brightness(self, brightness: int, effect: str = "smooth", duration: int = 500) -> Optional[Dict]:
        """控制灯泡亮度"""
        if not (1 <= brightness <= 100):
            logger.error("亮度值必须在1-100之间")
            return None
        return self.send_command("set_bright", [brightness, effect, duration])

    def set_rgb(self, red: int, green: int, blue: int, effect: str = "smooth", duration: int = 500) -> Optional[Dict]:
        """控制灯泡RGB颜色"""
        rgb_value = (red << 16) | (green << 8) | blue
        if not (0 <= rgb_value <= 16777215):
            logger.error("RGB值超出范围（0-16777215）")
            return None
        return self.send_command("set_rgb", [rgb_value, effect, duration])

    def get_property(self, props: List[str]) -> Optional[Dict]:
        """获取灯泡当前状态"""
        valid_props = ["power", "bright", "ct", "rgb", "hue", "sat", "color_mode", "flowing", "delayoff"]
        for prop in props:
            if prop not in valid_props:
                logger.error(f"无效属性: {prop}，可选属性：{valid_props}")
                return None
        return self.send_command("get_prop", props)

    def close(self):
        """关闭TCP连接"""
        if self.is_connected and self.tcp_socket:
            try:
                self.tcp_socket.close()
                logger.info(f"已断开与{self.device_ip}:{self.device_port}的连接")
            except Exception as e:
                logger.error(f"关闭连接失败: {str(e)}")
            finally:
                self.is_connected = False
                self.tcp_socket = None


# ------------------------------ MQTT 服务器相关类 ------------------------------
class MessageHandler:
    """消息处理器，负责注册和管理处理函数"""
    
    def __init__(self):
        self.handlers: Dict[str, Callable] = {}
    
    def register(self, message_type: str) -> Callable:
        """装饰器，用于注册消息处理函数"""
        def decorator(func: Callable) -> Callable:
            self.handlers[message_type] = func
            logger.info(f"Registered handler for message type: {message_type}")
            return func
        return decorator
    
    def handle(self, message_type: str, data: Any) -> Any:
        """根据消息类型调用相应的处理函数"""
        if message_type in self.handlers:
            try:
                return self.handlers[message_type](data)
            except Exception as e:
                logger.error(f"Error handling message type {message_type}: {str(e)}")
                return {"status": "error", "message": str(e)}
        else:
            logger.warning(f"No handler found for message type: {message_type}")
            return {"status": "error", "message": f"No handler for {message_type}"}


class MessageParser:
    """消息解析器，负责解析原始消息"""
    
    @staticmethod
    def parse(raw_message: str) -> Dict[str, Any]:
        """解析JSON格式的消息"""
        try:
            message = json.loads(raw_message)
            if "type" not in message or "data" not in message:
                raise ValueError("Message must contain 'type' and 'data' fields")
            return message
        except json.JSONDecodeError:
            logger.error("Failed to decode JSON message")
            raise
        except ValueError as e:
            logger.error(f"Invalid message format: {str(e)}")
            raise


class IoTServer:
    """物联网服务端主类，集成灯泡控制功能"""
    
    def __init__(self, config: Dict[str, Any]):
        self.config = config
        self.client = mqtt.Client()
        self.message_handler = MessageHandler()
        self.message_parser = MessageParser()
        self.light_client: Optional[YeelightClient] = None  # 灯泡客户端实例
        self.light_state = {  # 记录当前灯泡状态
            "power": "off",
            "brightness": 100,
            "rgb": (255, 255, 255),
            "last_updated": None
        }
        
        # 设置MQTT回调
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        
        # 如果配置了用户名密码
        if "username" in config and "password" in config:
            self.client.username_pw_set(
                config["username"], 
                config["password"]
            )

        # 初始化灯泡连接
        self._init_light_connection()
    
    def _init_light_connection(self):
        """初始化灯泡连接（发现并连接第一个设备）"""
        devices = YeelightDiscoverer.discover(timeout=5)
        if not devices:
            logger.warning("未发现任何Yeelight设备，请检查局域网控制是否开启")
            return
        
        # 选择第一个设备连接
        target_device = devices[0]
        logger.info(f"选中设备：{target_device}")
        self.light_client = YeelightClient(
            device_ip=target_device["ip"],
            device_port=target_device["port"]
        )
        # 连接时注册状态更新回调
        self.light_client.connect(notification_callback=self._update_light_state)
        
        # 初始获取灯泡状态
        self._refresh_light_state()
    
    def _update_light_state(self, status: Dict):
        """更新灯泡状态（处理灯泡主动推送的通知）"""
        with threading.Lock():
            if "power" in status:
                self.light_state["power"] = status["power"]
            if "bright" in status:
                self.light_state["brightness"] = int(status["bright"])
            if "rgb" in status:
                rgb_val = int(status["rgb"])
                self.light_state["rgb"] = (
                    (rgb_val >> 16) & 0xFF,
                    (rgb_val >> 8) & 0xFF,
                    rgb_val & 0xFF
                )
            self.light_state["last_updated"] = time.time()
        logger.info(f"灯泡状态已更新: {self.light_state}")
    
    def _refresh_light_state(self):
        """主动刷新灯泡状态"""
        if self.light_client and self.light_client.is_connected:
            try:
                response = self.light_client.get_property(["power", "bright", "rgb"])
                if response and "result" in response:
                    power, bright, rgb = response["result"]
                    self._update_light_state({
                        "power": power,
                        "bright": bright,
                        "rgb": rgb
                    })
            except Exception as e:
                logger.error(f"刷新灯泡状态失败: {str(e)}")
    
    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Dict, rc: int):
        """MQTT连接成功回调"""
        if rc == 0:
            logger.info("Connected to MQTT broker successfully")
            for topic in self.config["topics"]:
                client.subscribe(topic)
                logger.info(f"Subscribed to topic: {topic}")
        else:
            logger.error(f"Failed to connect, return code {rc}")
    
    def _on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage):
        """MQTT消息接收回调"""
        logger.info(f"Received message from topic {msg.topic}")
        
        # 使用线程处理消息，避免阻塞MQTT客户端
        threading.Thread(
            target=self._process_message,
            args=(msg.payload.decode(),),
            daemon=True
        ).start()
    
    def _on_disconnect(self, client: mqtt.Client, userdata: Any, rc: int):
        """MQTT断开连接回调"""
        if rc != 0:
            logger.warning(f"Unexpected disconnection with rc {rc}")
        else:
            logger.info("Disconnected from MQTT broker")
    
    def _process_message(self, raw_message: str):
        """处理接收到的消息"""
        try:
            message = self.message_parser.parse(raw_message)
            message_type = message["type"]
            data = message["data"]
            
            logger.info(f"Processing message of type: {message_type}")
            result = self.message_handler.handle(message_type, data)
            logger.info(f"Message processing result: {result}")
            
        except Exception as e:
            logger.error(f"Error processing message: {str(e)}")
    
    def start(self):
        """启动服务"""
        logger.info("Starting IoT server...")
        self.client.connect(
            self.config["broker_host"],
            self.config["broker_port"],
            keepalive=60
        )
        self.client.loop_forever()
    
    def stop(self):
        """停止服务"""
        logger.info("Stopping IoT server...")
        if self.light_client:
            self.light_client.close()
        self.client.disconnect()


# ------------------------------ 程序入口 ------------------------------
if __name__ == "__main__":
    # 配置
    config = {
        "broker_host": "localhost",
        "broker_port": 1883,
        "username": "",  # MQTT broker用户名（可选）
        "password": "",  # MQTT broker密码（可选）
        "topics": ["iot/devices/#"]  # 订阅的MQTT主题
    }
    
    # 创建服务器实例（会自动发现并连接灯泡）
    server = IoTServer(config)
    
    # 注册消息处理函数（处理设备数据和控制命令）
    @server.message_handler.register("device_data")
    def handle_device_data(data):
        """处理设备发送的数据信息（格式：{"name": "属性名", "value": 值}）"""
        if not isinstance(data, dict) or "name" not in data or "value" not in data:
            return {"status": "error", "message": "Invalid data format"}
        
        logger.info(f"Received device data: {data['name']} = {data['value']}")
        return {"status": "success", "message": "Data processed"}
    
    @server.message_handler.register("control_command")
    def handle_control_command(data):
        """处理控制命令（格式：{"command": "命令名", ...参数...}）"""
        if not isinstance(data, dict) or "command" not in data:
            return {"status": "error", "message": "Invalid command format"}
        
        if not server.light_client or not server.light_client.is_connected:
            return {"status": "error", "message": "Light not connected"}
        
        command = data["command"]
        try:
            if command == "turn_on":
                response = server.light_client.set_power("on")
            elif command == "turn_off":
                response = server.light_client.set_power("off")
            elif command == "set_brightness" and "value" in data:
                response = server.light_client.set_brightness(int(data["value"]))
            elif command == "set_color" and "r" in data and "g" in data and "b" in data:
                response = server.light_client.set_rgb(
                    int(data["r"]), int(data["g"]), int(data["b"])
                )
            elif command == "get_state":
                return {"status": "success", "data": server.light_state}
            else:
                return {"status": "error", "message": f"Unknown command: {command}"}
            
            return {"status": "success", "response": response}
        except Exception as e:
            return {"status": "error", "message": str(e)}
    
    # 启动服务器
    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()