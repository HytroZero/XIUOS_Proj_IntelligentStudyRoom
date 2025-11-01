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
    
    // 5. 可选：等待一段时间确保连接稳定
    PrivTaskDelay(3000);
    
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
int CreateAndStartSensorTasks(void)
{
    UtaskType temp_task, humi_task;
    
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
        UserTaskDelete(temperature_task_id);
        return -1;
    }
    
    /* 启动任务 */
    if (UserTaskStartup(temperature_task_id) != EOK) {
        printf(" Failed to start temperature task\n");
        return -1;
    }
    
    UserTaskDelay(1000);
    if (UserTaskStartup(humidity_task_id) != EOK) {
        printf(" Failed to start humidity task\n");
        UserTaskDelete(temperature_task_id);
        return -1;
    }
    
    printf(" Sensor tasks created successfully:\n");
    printf("   - Temperature Task: ID=%d, Priority=%d\n", temperature_task_id, TEMPERATURE_TASK_PRIORITY);
    printf("   - Humidity Task: ID=%d, Priority=%d\n", humidity_task_id, HUMIDITY_TASK_PRIORITY);
    
    return 0;
}

/**
 * @description: 停止传感器任务
 */
void StopSensorTasks(void)
{
    printf(" Stopping sensor tasks...\n");
    
    temperature_task_run = 0;
    humidity_task_run = 0;
    
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
    
    printf("✅ Sensor tasks stopped successfully\n");
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