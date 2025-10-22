/*
 * Copyright (c) 2024 AIIT XUOS Lab
 * XiUOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file temperature_dht22.c
 * @brief DHT22 temperature example
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2024.01.23
 */

#include <sensor.h>

/**
 * @description: 读取DHT22温度传感器数据
 * @return 0
 */
void TempDht22(void)
{
    int i = 0;
    int32_t temperature;
    float humidity;
    
    /* 查找并打开DHT22温度传感器 */
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_DHT22_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    if (temp == NULL) {
        printf("DHT22 temperature sensor not found!\n");
        return;
    }
    
    SensorQuantityOpen(temp);
    printf("DHT22 temperature sensor opened successfully\n");
    
    /* 读取10次温度数据，每次间隔2秒（符合DHT22采样要求） */
    for (i = 0; i < 10; i++) {
        temperature = SensorQuantityReadValue(temp);
        
        if (temperature != SENSOR_QUANTITY_VALUE_ERROR) {
            /* 处理正负温度显示 */
            if (temperature >= 0) {
                printf("Sample %d: Temperature : %d.%d ℃", 
                       i + 1, temperature / 10, temperature % 10);
            } else {
                printf("Sample %d: Temperature : -%d.%d ℃", 
                       i + 1, -temperature / 10, -temperature % 10);
            }
            
            /* 同时解析湿度数据（从传感器缓冲区） */
            if (temp->sdev && temp->sdev->buffer[0] != 0) {
                uint16_t humi_raw = (temp->sdev->buffer[0] << 8) | temp->sdev->buffer[1];
                humidity = (float)humi_raw * 0.1;
                printf(", Humidity : %.1f%% RH", humidity);
            }
            printf("\n");
        } else {
            printf("Sample %d: Temperature read error!\n", i + 1);
        }
        
        /* DHT22要求采样间隔至少2秒 */
        PrivTaskDelay(2000);
    }
    
    SensorQuantityClose(temp);
    printf("DHT22 temperature sensor closed\n");
}

/**
 * @description: 高精度温度读取示例（带数据校验）
 * @return 0
 */
void TempDht22HighPrecision(void)
{
    int i, j;
    int32_t temperature;
    int success_count = 0;
    const int max_retry = 3;
    
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_DHT22_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    if (temp == NULL) {
        printf("DHT22 temperature sensor not found!\n");
        return;
    }
    
    SensorQuantityOpen(temp);
    printf("Starting high-precision DHT22 temperature measurement...\n");
    
    for (i = 0; i < 5; i++) {
        /* 重试机制，提高数据可靠性 */
        for (j = 0; j < max_retry; j++) {
            temperature = SensorQuantityReadValue(temp);
            
            if (temperature != SENSOR_QUANTITY_VALUE_ERROR) {
                success_count++;
                
                /* 详细温度信息输出 */
                printf("Measurement %d-%d: ", i + 1, j + 1);
                if (temperature >= 0) {
                    printf("Temperature: %d.%d ℃", temperature / 10, temperature % 10);
                } else {
                    printf("Temperature: -%d.%d ℃", -temperature / 10, -temperature % 10);
                }
                
                /* 显示原始数据用于调试 */
                if (temp->sdev) {
                    printf(" [Raw: ");
                    for (int k = 0; k < 5; k++) {
                        printf("%02X", temp->sdev->buffer[k] & 0xFF);
                        if (k < 4) printf(" ");
                    }
                    printf("]");
                }
                printf("\n");
                break;
            } else {
                printf("Measurement %d-%d: Read failed, retrying...\n", i + 1, j + 1);
                PrivTaskDelay(1000);
            }
        }
        
        PrivTaskDelay(2000);
    }
    
    printf("Measurement completed: %d/%d successful readings\n", 
           success_count, i * max_retry);
    SensorQuantityClose(temp);
}

/**
 * @description: DHT22温度报警监控示例
 * @param low_threshold - 低温阈值（单位：0.1℃）
 * @param high_threshold - 高温阈值（单位：0.1℃）
 * @param duration - 监控持续时间（秒）
 */
void TempDht22Monitor(int32_t low_threshold, int32_t high_threshold, int duration)
{
    int32_t temperature;
    int alarm_count = 0;
    int total_readings = duration / 2;  /* 每2秒读取一次 */
    
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_DHT22_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    if (temp == NULL) {
        printf("DHT22 temperature sensor not found!\n");
        return;
    }
    
    SensorQuantityOpen(temp);
    printf("Starting temperature monitoring: Low=%d.%d℃, High=%d.%d℃, Duration=%ds\n",
           low_threshold/10, low_threshold%10, high_threshold/10, high_threshold%10, duration);
    
    for (int i = 0; i < total_readings; i++) {
        temperature = SensorQuantityReadValue(temp);
        
        if (temperature != SENSOR_QUANTITY_VALUE_ERROR) {
            /* 温度报警检查 */
            if (temperature < low_threshold) {
                printf("ALARM! Temperature too LOW: ");
                alarm_count++;
            } else if (temperature > high_threshold) {
                printf("ALARM! Temperature too HIGH: ");
                alarm_count++;
            } else {
                printf("Normal temperature: ");
            }
            
            /* 温度显示 */
            if (temperature >= 0) {
                printf("%d.%d ℃", temperature / 10, temperature % 10);
            } else {
                printf("-%d.%d ℃", -temperature / 10, -temperature % 10);
            }
            printf(" [%d/%d]\n", i + 1, total_readings);
        } else {
            printf("Reading %d/%d: Measurement error\n", i + 1, total_readings);
        }
        
        PrivTaskDelay(2000);
    }
    
    printf("Monitoring completed: %d alarm(s) detected\n", alarm_count);
    SensorQuantityClose(temp);
}

/**
 * @description: DHT22传感器诊断测试
 * @return 0
 */
void Dht22DiagnosticTest(void)
{
    printf("=== DHT22 Sensor Diagnostic Test ===\n");
    
    /* 1. 传感器检测 */
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_DHT22_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    if (temp == NULL) {
        printf("FAIL: DHT22 sensor not found\n");
        return;
    }
    printf("PASS: DHT22 sensor detected\n");
    
    /* 2. 传感器打开测试 */
    if (SensorQuantityOpen(temp) != 0) {
        printf("FAIL: Cannot open DHT22 sensor\n");
        return;
    }
    printf("PASS: DHT22 sensor opened successfully\n");
    
    /* 3. 连续读取测试 */
    int success_count = 0;
    for (int i = 0; i < 5; i++) {
        int32_t temperature = SensorQuantityReadValue(temp);
        if (temperature != SENSOR_QUANTITY_VALUE_ERROR) {
            success_count++;
            printf("Test %d: Temperature reading OK - ", i + 1);
            if (temperature >= 0) {
                printf("%d.%d ℃\n", temperature / 10, temperature % 10);
            } else {
                printf("-%d.%d ℃\n", -temperature / 10, -temperature % 10);
            }
        } else {
            printf("Test %d: Temperature reading FAILED\n", i + 1);
        }
        PrivTaskDelay(2000);
    }
    
    /* 4. 测试结果汇总 */
    printf("Diagnostic result: %d/%d successful readings\n", success_count, 5);
    if (success_count >= 3) {
        printf("OVERALL: PASS - DHT22 sensor working properly\n");
    } else {
        printf("OVERALL: FAIL - DHT22 sensor may have issues\n");
    }
    
    SensorQuantityClose(temp);
    printf("=== Diagnostic Test Completed ===\n");
}

#ifdef USING_SHELL
#include <shell.h>

/* Shell命令接口 */
int Dht22TempTest(int argc, char *argv[])
{
    if (argc == 1) {
        TempDht22();
    } else if (argc == 2 && !strcmp(argv[1], "precision")) {
        TempDht22HighPrecision();
    } else if (argc == 2 && !strcmp(argv[1], "diagnostic")) {
        Dht22DiagnosticTest();
    } else if (argc == 4 && !strcmp(argv[1], "monitor")) {
        int32_t low_temp = atoi(argv[2]);
        int32_t high_temp = atoi(argv[3]);
        TempDht22Monitor(low_temp, high_temp, 60);
    } else {
        printf("Usage: dht22_temp [precision|diagnostic|monitor low high]\n");
    }
    return 0;
}

PRIV_SHELL_CMD_FUNCTION(Dht22TempTest, DHT22 temperature sensor test, PRIV_SHELL_CMD_MAIN_ATTR);

#endif