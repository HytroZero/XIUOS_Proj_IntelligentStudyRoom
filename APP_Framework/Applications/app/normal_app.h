#ifndef _NORMAL_APP_H
#define _NORMAL_APP_H
#include <adapter.h>
#include <adapter_wifi.h>
#include <cJSON.h>
#include "mqtt/MQTTPacket.h"
#include "mqtt/MQTTSubscribe.h"

#define WIFI_SSID       "4001"      
#define WIFI_PASSWORD   "nmsmshsa" 

/* 任务配置参数 */
#define MQTT_TASK_PRIORITY    20
#define TEMPERATURE_TASK_PRIORITY    20
#define HUMIDITY_TASK_PRIORITY       20
#define DETECT_TASK_PRIORITY       20
#define DETECT_RECEIVE_TASK_PRIORITY       20
#define SENSOR_TASK_STACK_SIZE      2048
#define MQTT_TASK_STACK_SIZE      4096
#define DETECT_TASK_STACK_SIZE      409600
#define DETECT_RECEIVE_TASK_PRIORITY   2048
#define SENSOR_RUN_CYCLES            10   /* 运行周期数，我改成死循环了 */
#define LOCK_TIMEOUT_MS             1000  /* 锁获取超时时间 */

static char mqtt_iot_ipaddr[] = {192, 168, 76, 154};
static char mqtt_iot_netmask[] = {255, 255, 255, 0};
static char mqtt_iot_gwaddr[] = {192, 168, 76, 136};

static char mqtt_socket_port_iot[] = "1883";
static char mqtt_ip_str_iot[] = "192.168.76.149";

// 设备状态枚举
typedef enum {
	STATE_NO_PERSON = 0,      // 无人状态
	STATE_PERSON_ENTER,       // 无人变有人状态
	STATE_PERSON_DARK,        // 有人且暗光状态
	STATE_PERSON_BRIGHT,      // 有人且明光状态
	STATE_HIGH_HUMIDITY,      // 湿度高状态
	STATE_HIGH_TEMPERATURE,   // 温度高状态
	STATE_NORMAL              // 正常状态
} DeviceState;

// 灯光控制命令结构
typedef struct {
	uint8_t power;      // 开关: 0-关, 1-开
	uint8_t brightness; // 亮度: 0-100%
	uint8_t color_temp; // 色温: 0-冷色(2700K), 1-中性(4000K), 2-暖色(5000K)
	uint8_t color_mode; // 颜色模式: 0-自动, 1-手动
} LightControl;

// 需要的辅助函数声明
typedef struct {
    uint8_t person_present;
    float light_intensity;
    float temperature;
    float humidity;
} SensorData;

static int32_t temperature_task_id = -1;
static int32_t humidity_task_id = -1;
static int32_t mqtt_task_id = -1;
static uint32_t detect_task_id = -1;
static uint32_t detect_receive_task_id = -1;

static uint8_t temperature_task_run = 1;
static uint8_t humidity_task_run = 1;
static uint8_t mqtt_task_run = 1;
static uint8_t detect_task_run = 1;
static uint8_t detect_receive_task_run = 1;

void TemperatureTask(void *parameter);
void HumidityTask(void *parameter);
void MqttEdgeDeviceTask();
void DetectTask(void *parameter);
void ReceiveDetectTask(void *parameter);
int CreateAndStartTasks(void);
void StopSensorTasks(void);
void MonitorSensorTasks(void);

int WifiInitAndConnect(void);
SensorData GetSensorDataFromQueue(void);
void GenerateLightControl(DeviceState state, LightControl *ctrl);
void PublishDeviceStatus(int fd, SensorData sensor_data, DeviceState state, LightControl light_ctrl);
const char* GetCurrentTimestamp(void);

// Tests >>>>>>
uint8_t CheckPersonPresence(void);
float GetLightIntensity(void);
float GetTemperature(void);
float GetHumidity(void);

#endif