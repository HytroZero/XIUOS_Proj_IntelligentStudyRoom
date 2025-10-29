/*
* Copyright (c) 2020 AIIT XUOS Lab
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
 * @file sht20_humi.c
 * @brief sht20 humidity driver base sensor
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2025.10.29
 */
#include <sensor.h>

#define SHT20_CMD_HUMI_HOLD           0xE5

static struct SensorDevice sht20;

static struct SensorProductInfo humidity_info = {
    (SENSOR_ABILITY_HUMI | SENSOR_ABILITY_TEMP),
    "Sensirion", 
    "SHT20",
};

/**
 * @description: 打开SHT20湿度传感器设备
 * @param sdev - 传感器设备指针
 * @return 成功: 0 错误: -1
 */
static int HumidityDeviceOpen(struct SensorDevice *sdev) {
    int result;
    uint16_t i2c_dev_addr = SENSOR_DEVICE_SHT20_I2C_ADDR;
    
    sdev->fd = PrivOpen(SENSOR_DEVICE_SHT20_DEV, O_RDWR);
    if (sdev->fd < 0) {
        printf("open %s error\n", SENSOR_DEVICE_SHT20_DEV);
        return -1;
    }

    struct PrivIoctlCfg ioctl_cfg;
    ioctl_cfg.ioctl_driver_type = I2C_TYPE;
    ioctl_cfg.args = &i2c_dev_addr;
    result = PrivIoctl(sdev->fd, OPE_INT, &ioctl_cfg);

    return result;
}

/**
 * @description: 读取湿度传感器数据
 * @param sdev - 传感器设备指针
 * @param len - 读取数据长度
 * @return 成功: 0, 失败: -1
 */
static int HumidityDeviceRead(struct SensorDevice *sdev, size_t len) {
    if (PrivRead(sdev->fd, sdev->buffer, len) < 0)
        return -1;
    return 0;
}

/**
 * @description: 写入湿度传感器命令
 * @param sdev - 传感器设备指针
 * @param buf - 写入数据缓冲区
 * @param len - 写入数据长度
 * @return 成功: 0, 失败: -1
 */
static int HumidityDeviceWrite(struct SensorDevice *sdev, const void *buf, size_t len) {
    if (PrivWrite(sdev->fd, buf, len) < 0)
        return -1;
    return 0;
}

static struct SensorDone humidity_done = {
    HumidityDeviceOpen,
    NULL,
    HumidityDeviceRead,
    HumidityDeviceWrite,
    NULL,
};

/**
 * @description: 初始化SHT20湿度传感器并注册
 */
static void SensorDeviceSht20HumidityInit(void) {
    sht20.name = SENSOR_DEVICE_SHT20 ;
    sht20.info = &humidity_info;
    sht20.done = &humidity_done;
    sht20.status = SENSOR_DEVICE_PASSIVE;

    SensorDeviceRegister(&sht20);
}

static struct SensorQuantity sht20_humidity;

/**
 * @description: 湿度信号转换为实际湿度值
 * @param raw_data - 原始湿度数据
 * @return 湿度值(%RH × 10)
 */
static float Sht20ConvertHumidity(uint16_t raw_data) {
    // 清除状态位(最后两位)
    raw_data &= 0xFFFC;
    
    // 根据数据手册公式转换: RH = -6 + 125 × S_RH / 2^16
    float humidity = -6.0 + 125.0 * (raw_data / 65536.0);
    
    // 限制湿度范围在0-100%RH之间
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;
    
    return humidity;
}

/**
 * @description: 读取SHT20湿度值
 * @param quant - 传感器量指针
 * @return 湿度值(%RH × 10)
 */
static int32_t ReadHumidity(struct SensorQuantity *quant) {
    if (!quant)
        return -1;

    uint8_t humi_cmd = SHT20_CMD_HUMI_HOLD;
    float result;
    
    if (quant->sdev->done->read != NULL && quant->sdev->done->write != NULL) {
        if (quant->sdev->status == SENSOR_DEVICE_PASSIVE) {
            // 发送湿度测量命令
            if (quant->sdev->done->write(quant->sdev, &humi_cmd, 1) < 0) {
                printf("Send humidity command failed\n");
                return -1;
            }
            
            // 等待测量完成(最大22ms for 12bit分辨率)
            PrivTaskDelay(30);
            
            // 读取3字节数据(2字节数据 + 1字节CRC)
            if (quant->sdev->done->read(quant->sdev, 3) == 0) {
                // 组合湿度数据
                uint16_t raw_humi = (quant->sdev->buffer[0] << 8) | quant->sdev->buffer[1];
                
                // 转换湿度值
                result = Sht20ConvertHumidity(raw_humi);
                
                // printf("SHT20 Humidity - Raw: 0x%04X, Converted: %.1f%%RH\n", 
                //        raw_humi, result);
                
                // 返回湿度值(放大10倍)
                return (int32_t)(result * 10);
            } else {
                printf("Read humidity data failed\n");
            }
        } else {
            printf("Please set passive mode for humidity reading.\n");
        }
    } else {
        printf("%s don't have read/write done.\n", quant->name);
    }
    
    return -1;
}

/**
 * @description: 带温度补偿的湿度读取（更精确）
 * @param quant - 传感器量指针
 * @param temperature - 当前温度值(°C × 10)
 * @return 补偿后的湿度值(%RH × 10)
 */
static int32_t ReadHumidityWithCompensation(struct SensorQuantity *quant, int32_t temperature) {
    if (!quant)
        return -1;

    // 先读取原始湿度
    int32_t raw_humidity = ReadHumidity(quant);
    if (raw_humidity == -1) {
        return -1;
    }
    
    float humidity = raw_humidity / 10.0;  // 转换为%RH
    float temp_c = temperature / 10.0;     // 转换为°C
    
    // 根据数据手册进行温度补偿
    // 在高温高湿环境下进行补偿
    if (temp_c > 25.0 && humidity > 60.0) {
        // 简单的线性补偿（实际应用中应根据数据手册使用更精确的公式）
        float compensation = (temp_c - 25.0) * 0.1;
        humidity = humidity - compensation;
        
        printf("Applied temperature compensation: -%.1f%%RH\n", compensation);
    }
    
    // 确保湿度在有效范围内
    if (humidity < 0.0) humidity = 0.0;
    if (humidity > 100.0) humidity = 100.0;
    
    return (int32_t)(humidity * 10);
}

/**
 * @description: 读取湿度和温度的综合函数
 * @param quant - 传感器量指针
 * @param temperature - 输出温度值(°C × 10)
 * @param humidity - 输出湿度值(%RH × 10)
 * @return 成功: 0, 失败: -1
 */
static int ReadHumidityAndTemperature(struct SensorQuantity *quant, 
                                     int32_t *temperature, int32_t *humidity) {
    if (!quant || !temperature || !humidity)
        return -1;

    // 这里可以实现同时读取温湿度的逻辑
    // 根据SHT20数据手册，可以优化读取顺序
    
    *temperature = -1;
    *humidity = -1;
    
    // 实际应用中可能需要更复杂的读取策略
    return -1; // 暂不实现
}

/**
 * @description: 初始化SHT20湿度量程并注册
 * @return 成功: 0, 失败: -1
 */
int Sht20HumidityInit(void) {
    SensorDeviceSht20HumidityInit();
    
    sht20_humidity.name = SENSOR_QUANTITY_SHT20_HUMIDITY;
    sht20_humidity.type = SENSOR_QUANTITY_HUMI;
    sht20_humidity.value.decimal_places = 1;
    sht20_humidity.value.max_std = 1000;   // 0% to 100% RH
    sht20_humidity.value.min_std = 0;      // 0.0% RH
    sht20_humidity.value.last_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_humidity.value.max_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_humidity.value.min_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_humidity.sdev = &sht20;
    sht20_humidity.ReadValue = ReadHumidity;

    SensorQuantityRegister(&sht20_humidity);
    return 0;
}