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
    """MQTT服务端主类"""

    def __init__(self, config: Dict[str, Any]):
        client_id = time.strftime('%Y%m%d%H%M%S', time.localtime(time.time()))
        self.config = config
        self.client = mqtt.Client(client_id=client_id)
        self.message_handler = MessageHandler()
        self.message_parser = MessageParser()

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

    # 创建服务器实例
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
        logger.info(f"handle_control_command: {data}")


    # 启动服务器
    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()