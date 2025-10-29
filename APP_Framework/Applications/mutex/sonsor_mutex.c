#include "sensor_mutex.h"
#include <stdio.h>

int32_t sht20_mutex = -1;

/**
 * @description: 初始化传感器互斥锁
 * @return 成功: 0, 失败: -1
 */
int SensorMutexInit(void)
{
    if (sht20_mutex == -1) {
        sht20_mutex = UserMutexCreate();
        if (sht20_mutex < 0) {
            printf(" Failed to create sensor mutex\n");
            return -1;
        }
        printf(" Sensor mutex created successfully (ID: %d)\n", sht20_mutex);
    }
    return 0;
}

/**
 * @description: 销毁传感器互斥锁
 */
void SensorMutexDeinit(void)
{
    if (sht20_mutex != -1) {
        UserMutexDelete(sht20_mutex);
        sht20_mutex = -1;
        printf(" Sensor mutex destroyed\n");
    }
}

/**
 * @description: 获取传感器访问锁
 * @param wait_time 等待时间(毫秒)，WAIT_FOREVER表示无限等待
 * @return 成功: 0, 失败: -1
 */
int SensorLock(int32_t wait_time)
{
    if (sht20_mutex == -1) {
        printf(" Sensor mutex not initialized\n");
        return -1;
    }
    
    int32_t result = UserMutexObtain(sht20_mutex, wait_time);
    if (result != 0) {
        printf(" Failed to obtain sensor lock (Error: %d)\n", result);
        return -1;
    }
    
    return 0;
}

/**
 * @description: 释放传感器访问锁
 * @return 成功: 0, 失败: -1
 */
int SensorUnlock(void)
{
    if (sht20_mutex == -1) {
        printf(" Sensor mutex not initialized\n");
        return -1;
    }
    
    int32_t result = UserMutexAbandon(sht20_mutex);
    if (result != 0) {
        printf(" Failed to release sensor lock (Error: %d)\n", result);
        return -1;
    }
    
    return 0;
}

/**
 * @description: 检查传感器锁是否可用
 * @return 可用: 1, 不可用: 0
 */
int IsSensorLockAvailable(void)
{
    return (sht20_mutex != -1) ? 1 : 0;
}