#ifndef _NORMAL_APP_H
#define _NORMAL_APP_H
#include <adapter.h>
#include <adapter_wifi.h>

#define WIFI_SSID       "Factory"      
#define WIFI_PASSWORD   "00000000" 

/* 任务配置参数 */
#define TEMPERATURE_TASK_PRIORITY    20
#define HUMIDITY_TASK_PRIORITY       21
#define SENSOR_TASK_STACK_SIZE      2048
#define SENSOR_RUN_CYCLES            10   /* 运行周期数 */
#define LOCK_TIMEOUT_MS             1000  /* 锁获取超时时间 */

static int32_t temperature_task_id = -1;
static int32_t humidity_task_id = -1;
static uint8_t temperature_task_run = 1;
static uint8_t humidity_task_run = 1;

int WifiInitAndConnect(void);
void TemperatureTask(void *parameter);
void HumidityTask(void *parameter);
int CreateAndStartSensorTasks(void);
void StopSensorTasks(void);
void MonitorSensorTasks(void);

#endif