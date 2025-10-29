#ifndef _SENSOR_MUTEX_H
#define _SENSOR_MUTEX_H

#include "user_api.h"

/* 传感器互斥锁ID */
extern int32_t sht20_mutex;

/* 互斥锁操作函数 */
int SensorMutexInit(void);
void SensorMutexDeinit(void);
int SensorLock(int32_t wait_time);
int SensorUnlock(void);
int IsSensorLockAvailable(void);

#endif