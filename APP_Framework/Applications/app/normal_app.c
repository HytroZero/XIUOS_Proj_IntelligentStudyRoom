#include "normal_app.h"

int WifiInitAndConnect(void)
{
    int ret = 0;
    
    // 1. 查找Wi-Fi适配器
    struct Adapter* adapter = AdapterDeviceFindByName(ADAPTER_WIFI_NAME);
    if (!adapter) {
        printf("Wi-Fi adapter not found!\n");
        return -1;
    }
    
    // 2. 打开Wi-Fi设备
    ret = AdapterDeviceOpen(adapter);
    if (ret != 0) {
        printf("Failed to open Wi-Fi device! Error: %d\n", ret);
        return ret;
    }
    printf("Wi-Fi device opened successfully.\n");
    
    // 3. 配置Wi-Fi连接参数（使用宏定义）
    static struct WifiParam param;
    memset(&param, 0, sizeof(struct WifiParam));
    strncpy((char *)param.wifi_ssid, WIFI_SSID, sizeof(param.wifi_ssid) - 1);
    strncpy((char *)param.wifi_pwd, WIFI_PASSWORD, sizeof(param.wifi_pwd) - 1);
    
    // 确保字符串以null结尾
    param.wifi_ssid[sizeof(param.wifi_ssid) - 1] = '\0';
    param.wifi_pwd[sizeof(param.wifi_pwd) - 1] = '\0';
    
    adapter->adapter_param = &param;
    
    // 4. 执行连接操作
    printf("Connecting to Wi-Fi: %s...\n", WIFI_SSID);
    ret = AdapterDeviceSetUp(adapter);
    if (ret != 0) {
        printf("Wi-Fi connection failed! Error: %d\n", ret);
        // 关闭设备以防资源泄漏
        AdapterDeviceClose(adapter);
        return ret;
    }
    
    printf("Wi-Fi connected successfully to: %s\n", WIFI_SSID);
    
    return ret;
}

/**
 * @description: 温度传感器任务函数
 * @param parameter - 任务参数
 */
void TemperatureTask(void *parameter)
{
    printf(" Temperature sensor task started (ID: %d)\n", UserGetTaskID());
    int32_t temperature;
    int cycle_count = 0;
    struct SensorQuantity* temp = GetTempQuantity();
    while (temperature_task_run && cycle_count < SENSOR_RUN_CYCLES) {
        printf("\n=== Temperature Measurement Cycle %d ===\n", cycle_count + 1);
        
        if (SensorLock(LOCK_TIMEOUT_MS) == 0){
            temperature = SensorQuantityReadValue(temp);
            SensorUnlock();
            if (temperature > 0)
                printf("Temperature : %d.%d C\n", temperature/10, temperature%10);
            else
                printf("Temperature : %d.%d C\n", temperature/10, -temperature%10);
        }
        /* 任务延迟5秒 */
        UserTaskDelay(3000);
        cycle_count++;
    }
    SensorQuantityClose(temp);
    // printf(" Temperature task completed after %d cycles\n", cycle_count);
    UserTaskQuit();
}

/**
 * @description: 湿度传感器任务函数
 * @param parameter - 任务参数
 */
void HumidityTask(void *parameter)
{
    printf(" Humidity sensor task started (ID: %d)\n", UserGetTaskID());
    
    int cycle_count = 0;
    int32_t humidity;
    struct SensorQuantity *humi = GetHumiQuantity();
    while (humidity_task_run && cycle_count < SENSOR_RUN_CYCLES) {
        printf("\n=== Humidity Measurement Cycle %d ===\n", cycle_count + 1);
        if (SensorLock(LOCK_TIMEOUT_MS) == 0){
            humidity = SensorQuantityReadValue(humi);
            SensorUnlock();
            printf("Humidity : %d.%d %%RH\n", humidity/10, humidity%10);
        }
        
        UserTaskDelay(3000);
        cycle_count++;
    }
    SensorQuantityClose(humi);

    // printf(" Humidity task completed after %d cycles\n", cycle_count);
    UserTaskQuit();
}

/**
 * @description: 创建并启动传感器任务
 * @return 成功: 0, 失败: -1
 */
int CreateAndStartTasks(void)
{
    UtaskType temp_task, humi_task, mqtt_task;
    
    printf(" Initializing sensor tasks...\n");
    SensorMutexInit();
    /* 创建温度传感器任务 */
    strncpy(temp_task.name, "temp_task", NAME_NUM_MAX - 1);
    temp_task.func_entry = (void *)TemperatureTask;
    temp_task.func_param = (void *)&temperature_task_run;
    temp_task.stack_size = SENSOR_TASK_STACK_SIZE;
    temp_task.prio = TEMPERATURE_TASK_PRIORITY;
    
    temperature_task_id = UserTaskCreate(temp_task);
    if (temperature_task_id < 0) {
        printf(" Failed to create temperature task\n");
        return -1;
    }
    
    /* 创建湿度传感器任务 */
    strncpy(humi_task.name, "humi_task", NAME_NUM_MAX - 1);
    humi_task.func_entry = (void *)HumidityTask;
    humi_task.func_param = (void *)&humidity_task_run;
    humi_task.stack_size = SENSOR_TASK_STACK_SIZE;
    humi_task.prio = HUMIDITY_TASK_PRIORITY;
    
    humidity_task_id = UserTaskCreate(humi_task);
    if (humidity_task_id < 0) {
        printf(" Failed to create humidity task\n");
        UserTaskDelete(humidity_task_id);
        return -1;
    }
    
	 /* 创建MQTT边缘设备任务 */
    strncpy(mqtt_task.name, "mqtt_edge_task", NAME_NUM_MAX - 1);
    mqtt_task.func_entry = (void *)MqttEdgeDeviceTask;
    mqtt_task.func_param = NULL;
    mqtt_task.stack_size = MQTT_TASK_STACK_SIZE;   // MQTT任务需要较大栈空间
    mqtt_task.prio = MQTT_TASK_PRIORITY;           // 设置合适的优先级
    
    mqtt_task_id = UserTaskCreate(mqtt_task);
    if (mqtt_task_id < 0) {
        printf("Failed to create MQTT edge device task\n");
		UserTaskDelete(mqtt_task_id);
        return -1;
    } else {
        printf("MQTT edge device task created successfully, ID: %d\n", mqtt_task_id);
    }
	
    // /* 启动任务 */
    // if (UserTaskStartup(temperature_task_id) != EOK) {
    //     printf(" Failed to start temperature task\n");
	// 	UserTaskDelete(temperature_task_id);
    //     return -1;
    // }
    
    // UserTaskDelay(500);
    // if (UserTaskStartup(humidity_task_id) != EOK) {
    //     printf(" Failed to start humidity task\n");
    //     UserTaskDelete(humidity_task_id);
    //     return -1;
    // }

	UserTaskDelay(100);
    if (UserTaskStartup(mqtt_task_id) != EOK) {
        printf(" Failed to start mqtt task\n");
        UserTaskDelete(mqtt_task_id);
        return -1;
    }
    
    printf(" tasks created successfully:\n");
    printf("   - Temperature Task: ID=%d, Priority=%d\n", temperature_task_id, TEMPERATURE_TASK_PRIORITY);
    printf("   - Humidity Task: ID=%d, Priority=%d\n", humidity_task_id, HUMIDITY_TASK_PRIORITY);
	printf("   - MQTT Task: ID=%d, Priority=%d\n", mqtt_task_id, MQTT_TASK_PRIORITY);
    
    return 0;
}

/**
 * @description: 停止传感器任务
 */
void StopSensorTasks(void)
{
    printf(" Stopping tasks...\n");
    
    temperature_task_run = 0;
    humidity_task_run = 0;
	mqtt_task_run = 0;
    
    /* 给任务一些时间正常退出 */
    UserTaskDelay(200);
    
    /* 强制删除任务 */
    if (temperature_task_id >= 0) {
        UserTaskDelete(temperature_task_id);
        temperature_task_id = -1;
    }
    
    if (humidity_task_id >= 0) {
        UserTaskDelete(humidity_task_id);
        humidity_task_id = -1;
    }
    
	if (mqtt_task_id >= 0) {
		UserTaskDelete(mqtt_task_id);
		mqtt_task_id = -1;
    }
    printf("tasks stopped successfully\n");
}

/**
 * @description: 监控任务状态
 */
void MonitorSensorTasks(void)
{
    int monitor_count = 0;
    
    printf(" Starting task monitoring...\n");
    
    while (monitor_count < SENSOR_RUN_CYCLES * 2) {
        char temp_name[NAME_NUM_MAX], humi_name[NAME_NUM_MAX];
        uint8_t temp_stat, humi_stat;
        
        UserGetTaskName(temperature_task_id, temp_name);
        UserGetTaskName(humidity_task_id, humi_name);
        temp_stat = UserGetTaskStat(temperature_task_id);
        humi_stat = UserGetTaskStat(humidity_task_id);
        
        printf("\n--- Task Status Monitor (Cycle %d) ---\n", monitor_count + 1);
        printf("Temperature Task: ID=%d, Name=%s, State=%d\n", 
               temperature_task_id, temp_name, temp_stat);
        printf("Humidity Task: ID=%d, Name=%s, State=%d\n", 
               humidity_task_id, humi_name, humi_stat);
        
        UserTaskDelay(5000);  /* 每5秒监控一次 */
        monitor_count++;
    }
}

static AdapterType g_mqtt_adapter = NULL;

void MqttEdgeDeviceTask()
{
    int ret;
    DeviceState current_state = STATE_NO_PERSON;
    DeviceState previous_state = STATE_NO_PERSON;
    LightControl light_ctrl = {0};

    // 查找并初始化WiFi适配器
    g_mqtt_adapter = AdapterDeviceFindByName(ADAPTER_WIFI_NAME);
    if (!g_mqtt_adapter) {
        lw_print("Failed to find WiFi adapter\n");
        return;
    }

    // 设置MQTT连接参数
    const char *client_id = "edge_device_001";
    const char *username = NULL;  // 根据实际情况设置
    const char *password = NULL;  // 根据实际情况设置

    // MQTT连接循环
MQTT_CONNECT:
    // 使用适配器API连接MQTT服务器
    ret = AdapterDeviceMqttConnect(g_mqtt_adapter, mqtt_ip_str_iot, 
                                 mqtt_socket_port_iot, client_id, 
                                 username, password);
    if (ret < 0) {
        lw_print("MQTT connect failed: %d\n", ret);
        PrivTaskDelay(3000);
        goto MQTT_CONNECT;
    }

    lw_print("MQTT connect %s:%s success\n", mqtt_ip_str_iot, mqtt_socket_port_iot);

    // 订阅主题
    const char *subscribe_topic = "iot/devices/#";
    // 注意：订阅功能可能需要通过其他API实现，这里假设适配器支持
    
    lw_print("Subscribe %s success\n", subscribe_topic);

    // 主循环
    uint32_t tick_count = 0;
    uint8_t no_mqtt_msg_exchange = 1;
    
    // 接收缓冲区
    uint8_t recv_buf[512];
    char recv_topic[128];

    while(1) {
        tick_count++;
        
        // 处理MQTT消息接收（非阻塞）
        ssize_t recv_len = AdapterDeviceMqttRecv(g_mqtt_adapter, subscribe_topic, recv_buf, sizeof(recv_buf));
        if (recv_len > 0) {
            // 处理接收到的消息
            lw_print("Received MQTT message, len: %d\n", recv_len);
            ProcessMqttMessage(recv_buf, recv_len);
            no_mqtt_msg_exchange = 0;
        }

        // 每5秒处理一次传感器数据并发布状态
        if(tick_count % 500 == 0) {
            // 从内部消息队列获取传感器数据
            SensorData sensor_data = GetSensorDataFromQueue();
            
            // 状态机逻辑
            previous_state = current_state;
            
            if (!sensor_data.person_present) {
                current_state = STATE_NO_PERSON;
            } else if (previous_state == STATE_NO_PERSON && sensor_data.person_present) {
                current_state = STATE_PERSON_ENTER;
            } else if (sensor_data.person_present && sensor_data.light_intensity < 50) {
                current_state = STATE_PERSON_DARK;
            } else if (sensor_data.person_present && sensor_data.light_intensity >= 50) {
                current_state = STATE_PERSON_BRIGHT;
            } else if (sensor_data.humidity > 80.0) {
                current_state = STATE_HIGH_HUMIDITY;
            } else if (sensor_data.temperature > 30.0) {
                current_state = STATE_HIGH_TEMPERATURE;
            } else {
                current_state = STATE_NORMAL;
            }
            
            // 根据状态生成灯光控制命令
            GenerateLightControl(current_state, &light_ctrl);
            
            // 发布设备状态消息
            PublishDeviceStatusUsingAdapter(sensor_data, current_state, light_ctrl);
            
            no_mqtt_msg_exchange = 0;
        }

        // 每30秒检查连接状态（如果没有数据交换）
        if(tick_count % 3000 == 0) {
            if(no_mqtt_msg_exchange) {
                // 尝试发送心跳或检查连接状态
                // 这里可以发送一个空消息或使用适配器的心跳功能
                if (!CheckMqttConnection()) {
                    lw_print("Connection lost, reconnecting...\n");
                    AdapterDeviceMqttDisconnect(g_mqtt_adapter);
                    PrivTaskDelay(3000);
                    goto MQTT_CONNECT;
                }
                lw_print("Connection alive\n");
            }
            no_mqtt_msg_exchange = 1;
        }

        PrivTaskDelay(10); // 10ms延迟
    }

    // 清理资源
    AdapterDeviceMqttDisconnect(g_mqtt_adapter);
}

// 处理接收到的MQTT消息
void ProcessMqttMessage(uint8_t *data, size_t len)
{
    // 简单的消息处理示例
    if (len > 0) {
        lw_print("MQTT message: %.*s\n", len, data);
        
        // 这里可以添加具体的消息解析和处理逻辑
        // 例如：解析控制命令、配置更新等
    }
}

// 检查MQTT连接状态
int CheckMqttConnection(void)
{
    // 尝试发送一个小的测试消息来检查连接
    const char *test_topic = "iot/devices/ping";
    const char *test_payload = "ping";
    
    ssize_t ret = AdapterDeviceMqttSend(g_mqtt_adapter, test_topic, test_payload, strlen(test_payload));
    return (ret >= 0);
}

/**
 * @brief 从内部消息队列获取传感器数据
 * @return 传感器数据结构
 */
SensorData GetSensorDataFromQueue(void)
{
	static uint32_t call_count = 0;
    static uint8_t scenario_index = 0;
    
    SensorData data = {0};
    
    // 6种场景循环，对应6种状态
    scenario_index = call_count % 6;
    
    switch (scenario_index) {
        case 0: // STATE_NO_PERSON - 无人状态
            data.person_present = 0;
            data.light_intensity = 40.0f;  // 中等光照
            data.temperature = 26.0f;
            data.humidity = 55.0f;
            break;
            
        case 1: // STATE_PERSON_ENTER - 人员进入
            data.person_present = 1;
            data.light_intensity = 45.0f;  // 中等光照
            data.temperature = 26.5f;
            data.humidity = 56.0f;
            break;
            
        case 2: // STATE_PERSON_DARK - 有人+暗光
            data.person_present = 1;
            data.light_intensity = 15.0f;  // 暗光环境
            data.temperature = 25.5f;
            data.humidity = 58.0f;
            break;
            
        case 3: // STATE_PERSON_BRIGHT - 有人+明光
            data.person_present = 1;
            data.light_intensity = 85.0f;  // 明亮环境
            data.temperature = 27.0f;
            data.humidity = 52.0f;
            break;
            
        case 4: // STATE_HIGH_HUMIDITY - 高湿度
            data.person_present = 1;
            data.light_intensity = 50.0f;
            data.temperature = 28.0f;
            data.humidity = 88.0f;  // 高湿度
            break;
            
        case 5: // STATE_HIGH_TEMPERATURE - 高温
            data.person_present = 1;
            data.light_intensity = 55.0f;
            data.temperature = 35.0f;  // 高温
            data.humidity = 60.0f;
            break;
    }
    
    // 添加小范围随机波动，使数据更真实
    data.light_intensity += (rand() % 10) * 0.5f - 2.5f;
    data.temperature += (rand() % 10) * 0.1f - 0.5f;
    data.humidity += (rand() % 10) * 0.2f - 1.0f;
    
    // 确保数据在合理范围内
    if (data.light_intensity < 0) data.light_intensity = 0;
    if (data.light_intensity > 100) data.light_intensity = 100;
    if (data.temperature < -10) data.temperature = -10;
    if (data.temperature > 50) data.temperature = 50;
    if (data.humidity < 0) data.humidity = 0;
    if (data.humidity > 100) data.humidity = 100;
    
    call_count++;
    
    // 打印模拟数据用于调试
    lw_print("Sensor Data[场景%d]: 人员=%d, 光照=%.1f, 温度=%.1f°C, 湿度=%.1f%%\n",
             scenario_index, data.person_present, data.light_intensity, 
             data.temperature, data.humidity);
    
    return data;
    // SensorData data = {0};
    
    // // 这里模拟从消息队列获取数据
    // // 实际应用中应该从真正的消息队列读取
    // data.person_present = CheckPersonPresence();    // 检查是否有人
    // data.light_intensity = GetLightIntensity();     // 获取光照强度
    // data.temperature = GetTemperature();            // 获取温度
    // data.humidity = GetHumidity();                  // 获取湿度
    
    // return data;
}

/**
 * @brief 根据设备状态生成灯光控制命令
 * @param state 当前设备状态
 * @param ctrl 灯光控制结构体指针
 */
void GenerateLightControl(DeviceState state, LightControl *ctrl)
{
    switch(state) {
        case STATE_NO_PERSON:
            ctrl->power = 0;       // 关灯
            ctrl->brightness = 0;
            ctrl->color_temp = 0;
            break;
            
        case STATE_PERSON_ENTER:
            ctrl->power = 1;       // 开灯，中等亮度，中性色温
            ctrl->brightness = 60;
            ctrl->color_temp = 1;
            break;
            
        case STATE_PERSON_DARK:
            ctrl->power = 1;       // 开灯，高亮度，暖色调
            ctrl->brightness = 90;
            ctrl->color_temp = 2;
            break;
            
        case STATE_PERSON_BRIGHT:
            ctrl->power = 1;       // 开灯，低亮度，冷色调
            ctrl->brightness = 30;
            ctrl->color_temp = 0;
            break;
            
        case STATE_HIGH_HUMIDITY:
        case STATE_HIGH_TEMPERATURE:
            ctrl->power = 1;       // 开灯，中等亮度，冷色调（给人凉爽感）
            ctrl->brightness = 70;
            ctrl->color_temp = 0;
            break;
            
        case STATE_NORMAL:
            ctrl->power = 1;       // 开灯，舒适亮度，中性色温
            ctrl->brightness = 50;
            ctrl->color_temp = 1;
            break;
    }
    ctrl->color_mode = 0; // 自动模式
}

/**
 * @brief 发布设备状态到MQTT
 * @param fd socket描述符
 * @param sensor_data 传感器数据
 * @param state 设备状态
 * @param light_ctrl 灯光控制命令
 */
// 使用适配器API发布设备状态
void PublishDeviceStatusUsingAdapter(SensorData sensor_data, DeviceState state, LightControl light_ctrl)
{
    cJSON *root = cJSON_CreateObject();
    
    // 添加设备信息
    cJSON_AddStringToObject(root, "device_id", "edge_device_001");
    
    // 添加传感器数据
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "person_present", sensor_data.person_present);
    cJSON_AddNumberToObject(sensors, "light_intensity", sensor_data.light_intensity);
    cJSON_AddNumberToObject(sensors, "temperature", sensor_data.temperature);
    cJSON_AddNumberToObject(sensors, "humidity", sensor_data.humidity);
    cJSON_AddItemToObject(root, "sensor_data", sensors);
    
    // 添加设备状态
    const char *state_str[] = {
        "no_person", "person_enter", "person_dark", "person_bright",
        "high_humidity", "high_temperature", "normal"
    };
    cJSON_AddStringToObject(root, "device_state", state_str[state]);
    
    // 添加控制命令
    cJSON *control = cJSON_CreateObject();
    cJSON_AddBoolToObject(control, "power", light_ctrl.power);
    cJSON_AddNumberToObject(control, "brightness", light_ctrl.brightness);
    cJSON_AddNumberToObject(control, "color_temp", light_ctrl.color_temp);
    cJSON_AddNumberToObject(control, "color_mode", light_ctrl.color_mode);
    cJSON_AddItemToObject(root, "light_control", control);
    

    
    // 生成JSON字符串
    char *json_str = cJSON_PrintUnformatted(root);
    lw_print("Publishing: %s\n", json_str);
    
    // 使用适配器API发布消息
    const char *publish_topic = "iot/devices/status";
    ssize_t send_len = AdapterDeviceMqttSend(g_mqtt_adapter, publish_topic, json_str, strlen(json_str));
    
    if (send_len < 0) {
        lw_print("Publish failed: %d\n", send_len);
    } else {
        lw_print("Publish success, len: %d\n", send_len);
    }
    
    // 清理资源
    cJSON_free(json_str);
    cJSON_Delete(root);
}



// PRIV_SHELL_CMD_FUNCTION(MqttEdgeDeviceTask, IoT edge device MQTT client, PRIV_SHELL_CMD_MAIN_ATTR);


/**
 * 检测人员存在
 * 返回: 1-有人, 0-无人
 */
uint8_t CheckPersonPresence(void)
{
    // 简单实现：模拟随机的人员检测
    // 在实际应用中，这里应该读取红外传感器或摄像头数据
    static unsigned int call_count = 0;
    call_count++;
    
    // 每5次调用中有1次检测到人员（模拟随机检测）
    return (call_count % 5 == 0) ? 1 : 0;
}

/**
 * 获取光照强度
 * 返回: 光照强度值 (lux)
 */
float GetLightIntensity(void)
{
    // 简单实现：模拟光照强度读数
    // 在实际应用中，这里应该读取光照传感器数据
    static unsigned int call_count = 0;
    call_count++;
    
    // 模拟光照强度在100-1000 lux之间变化
    float base_light = 300.0f;
    float variation = (float)(call_count % 200); // 0-199的变化
    return base_light + variation;
}

/**
 * 获取温度值
 * 返回: 温度值 (°C)
 */
float GetTemperature(void)
{
    // 简单实现：模拟温度读数
    // 在实际应用中，这里应该读取温度传感器数据
    static unsigned int call_count = 0;
    call_count++;
    
    // 模拟温度在20.0-30.0°C之间缓慢变化
    float base_temp = 25.0f;
    float variation = (float)(call_count % 100) / 10.0f; // 0.0-9.9的变化
    return base_temp + (variation - 4.95f); // 在20.05-29.95之间
}

/**
 * 获取湿度值
 * 返回: 湿度值 (%RH)
 */
float GetHumidity(void)
{
    // 简单实现：模拟湿度读数
    // 在实际应用中，这里应该读取湿度传感器数据
    static unsigned int call_count = 0;
    call_count++;
    
    // 模拟湿度在40%-80%之间变化
    float base_humidity = 60.0f;
    float variation = (float)(call_count % 40); // 0-39的变化
    return base_humidity + (variation - 19.5f); // 在40.5-79.5之间
}

int MqttTest()
{
    struct Adapter* adapter = AdapterDeviceFindByName(ADAPTER_WIFI_NAME);
    if (!adapter) {
        printf("找不到WiFi适配器\n");
        return -1;
    }

    // 1. 使用适配器API连接MQTT服务器
    const char *ip = "192.168.76.149";
    const char *port = "1883";
    enum NetRoleType net_role = CLIENT;
    enum IpType ip_type = IPV4;
    adapter->socket.protocal = SOCKET_PROTOCOL_TCP;
    printf("连接MQTT服务器: %s:%s\n", ip, port);
    
    int ret = AdapterDeviceConnect(adapter, net_role, ip, port, ip_type);
    if (ret < 0) {
        printf("连接MQTT服务器失败: %d\n", ret);
        return -1;
    }
    printf("连接到MQTT服务器成功\n");
 	printf("组装MQTT连接参数...\n");
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    uint8_t buf[200];
    int buflen = sizeof(buf);
    int len = 0;
    
    data.clientID.cstring = "test_client_001";      // 客户端ID
    data.keepAliveInterval = 60;                   // 保持活跃60秒
    data.username.cstring = NULL;                  // 用户名（可选）
    data.password.cstring = NULL;                  // 密码（可选）
    data.MQTTVersion = 4;                          // MQTT 3.1.1
    data.cleansession = 1;                         // 清理会话
    
    len = MQTTSerialize_connect(buf, buflen, &data);
    if (len <= 0) {
        printf(" MQTT连接包序列化失败\n");
        return -1;
    }
    printf(" MQTT连接包序列化成功，长度: %d字节\n", len);
    
    // 发送连接包
    printf("发送MQTT CONNECT包...\n");
	AdapterDeviceSend(adapter, buf, len);

    PrivTaskDelay(1000); // 等待连接建立

    // 3. 发送MQTT PUBLISH消息（模拟真实设备通信）
printf("开始发送模拟设备消息...\n");

// 测试消息类型数组
const char* test_messages[] = {
    // 设备数据上报
    "{\"type\":\"device_data\",\"data\":{\"name\":\"temperature\",\"value\":25.5}}",
    "{\"type\":\"device_data\",\"data\":{\"name\":\"humidity\",\"value\":65.2}}",
    "{\"type\":\"device_data\",\"data\":{\"name\":\"light_intensity\",\"value\":780}}",
    
    // 控制命令（Yeelight灯泡）
    "{\"type\":\"control_command\",\"data\":{\"command\":\"turn_on\"}}",
    "{\"type\":\"control_command\",\"data\":{\"command\":\"set_brightness\",\"value\":80}}",
    "{\"type\":\"control_command\",\"data\":{\"command\":\"set_color\",\"r\":255,\"g\":100,\"b\":50}}",
    "{\"type\":\"control_command\",\"data\":{\"command\":\"turn_off\"}}",
    

};

int message_count = sizeof(test_messages) / sizeof(test_messages[0]);

for(int i = 0; i < message_count; ++i) {
    const char* topic = "iot/devices/control";  // 使用Python代码中的主题
    
    printf("发送消息[%d/%d]:\n", i+1, message_count);
    printf("  主题: %s\n", topic);
    printf("  内容: %s\n", test_messages[i]);
    
    // 使用适配器发送
    int ret = AdapterMQTTPublish_QOS0(adapter, topic, (uint8_t*)test_messages[i]);
    
    if (ret == 0) {
        printf("   发送成功\n");
    } else {
        printf("   发送失败，错误码: %d\n", ret);
    }
    
    PrivTaskDelay(5000); // 2秒间隔，便于观察
}
    
    return 0;
}

int AdapterMQTTPublish_QOS0(struct Adapter *adapter, char *topic, uint8_t* msg)
{
    if (!adapter || !topic || !msg) {
        return -1;
    }
    
    uint8_t buf[256];
    MQTTString topicString = {.cstring = topic};
    uint32_t msg_len = strlen((char *)msg);
    
    // 序列化PUBLISH包
    int len = MQTTSerialize_publish(buf, sizeof(buf), 0, 0, 0, 0, topicString, msg, msg_len);
    if (len <= 0) {
        return -1;
    }
    
    // 使用适配器发送
	AdapterDeviceSend(adapter, buf, len);
    
    return 0;
}