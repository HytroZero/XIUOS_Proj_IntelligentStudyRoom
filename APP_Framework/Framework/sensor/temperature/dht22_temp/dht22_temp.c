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
 * @file dht22_temp.c
 * @brief DHT22 temperature driver base sensor
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2024.01.22
 */

#include <sensor.h>

/* DHT22时序参数（单位：微秒）- 根据说明书精确设置 */
#define DHT22_START_SIGNAL_DURATION      500     /* 起始信号500us */
#define DHT22_RESPONSE_TIMEOUT           1000    /* 响应超时1ms */
#define DHT22_BIT_START_DURATION         50      /* 数据位起始50us */
#define DHT22_BIT_0_MAX_DURATION         28      /* 0位高电平<28us */
#define DHT22_BIT_1_MIN_DURATION         70      /* 1位高电平>70us */
#define DHT22_SAMPLING_INTERVAL_MS       2000    /* 采样间隔2秒 */
#define NULL_PARAMETER 0

static struct SensorDevice dht22;

/* 传感器信息 */
static struct SensorProductInfo info =
{
    (SENSOR_ABILITY_HUMI | SENSOR_ABILITY_TEMP),
    "Aosong",                   /* 厂商名称 */
    "DHT22",                    /* 传感器型号 */
};

/**
 * @description: 微秒级延时函数
 * @param us - 微秒数
 */
static void DHT22_DelayUs(uint32_t us)
{
    /* 根据CPU频率调整延时精度 */
    volatile uint32_t count = us * 10;  /* 假设10个循环≈1us */
    while (count--);
}

/**
 * @description: 配置GPIO引脚模式 - 基于参考代码修正
 * @param sdev - 传感器设备指针
 * @param mode - 引脚模式
 * @return success : EOK error : -1
 */
static int DHT22_ConfigPinMode(struct SensorDevice *sdev, int mode)
{
    struct PinParam pin_parameter;
    
    /* 完全初始化结构体 - 参考示例代码 */
    memset(&pin_parameter, 0, sizeof(struct PinParam));
    pin_parameter.cmd = GPIO_CONFIG_MODE;  /* 使用正确的配置命令 */
    pin_parameter.pin = SENSOR_DEVICE_DHT22_GPIO_PIN;
    pin_parameter.mode = mode;            /* 通过mode字段设置模式 */

    struct PrivIoctlCfg ioctl_cfg;
    ioctl_cfg.ioctl_driver_type = PIN_TYPE;
    ioctl_cfg.args = (void *)&pin_parameter;

    // printf("Configuring GPIO pin %ld to mode %d\n", pin_parameter.pin, mode);
    
    x_err_t ret = PrivIoctl(sdev->fd, OPE_CFG, &ioctl_cfg);
    if (ret != EOK) {
        printf("GPIO config failed: pin=%ld, mode=%d, ret=%ld\n", 
               pin_parameter.pin, mode, ret);
        return -1;
    }
    
    // printf("GPIO pin %ld mode configured successfully\n", pin_parameter.pin);
    return EOK;
}

/**
 * @description: 设置GPIO引脚电平 - 确保先设置模式
 * @param sdev - 传感器设备指针
 * @param level - 电平值 (0:低电平, 1:高电平)
 * @return success : 0 error : -1
 */
static int DHT22_SetPinLevel(struct SensorDevice *sdev, uint16_t level)
{
    struct PinStat pin_stat;
    
    /* 低电平输出，高电平输入 */
    if (DHT22_ConfigPinMode(sdev, GPIO_CFG_OUTPUT) != 0) {
        printf("[ERR] Cannot set output mode before level setting\n");
        return -1;
    }
    
    /* 设置引脚电平 */
    pin_stat.pin = SENSOR_DEVICE_DHT22_GPIO_PIN;
    pin_stat.val = level;
    
    // printf("Setting GPIO pin %ld to level %d\n", pin_stat.pin, level);
    
    if (PrivWrite(sdev->fd, &pin_stat, NULL_PARAMETER) < 0) {
        printf("[ERR] Set GPIO pin %ld level %d failed\n", pin_stat.pin, level);
        return -1;
    }
    
    // printf("GPIO pin %ld level set to %d successfully\n", pin_stat.pin, level);
    return 0;
}

/**
 * @description: 读取GPIO引脚电平 - 使用正确的struct PinStat
 * @param sdev - 传感器设备指针
 * @return 引脚电平值 (GPIO_LOW或GPIO_HIGH, -1表示错误)
 */
static int DHT22_ReadPinLevel(struct SensorDevice *sdev)
{
    struct PinStat pin_stat;
    
    pin_stat.pin = SENSOR_DEVICE_DHT22_GPIO_PIN;

    /* 使用PrivRead读取电平值 */
    if (PrivRead(sdev->fd, &pin_stat, NULL_PARAMETER) < 0) {
        printf("Read GPIO pin %ld level failed\n", pin_stat.pin);
        return -1;
    }
    
    // printf("GPIO pin %ld level read: %d\n", pin_stat.pin, pin_stat.val);
    return pin_stat.val;
}

/**
 * @description: 发送DHT22起始信号 - 严格按照说明书时序实现
 * @param sdev - 传感器设备指针
 * @return success : 0 error : -1
 */
static int DHT22_SendStartSignal(struct SensorDevice *sdev)
{
    int ret;
    
    // printf("Starting DHT22 communication on pin %d\n", SENSOR_DEVICE_DHT22_GPIO_PIN);
    
    /* 1. 拉低总线500us - 严格按照说明书时序要求 */
    ret = DHT22_SetPinLevel(sdev, GPIO_LOW);
    if (ret != 0) {
        printf("DHT22 set pin low failed\n");
        return -1;
    }

    /* 精确的500us延时 */
    DHT22_DelayUs(DHT22_START_SIGNAL_DURATION);
    
    
    /* 2. 释放总线（拉高） */
    ret = DHT22_SetPinLevel(sdev, GPIO_HIGH);
    if (ret != 0) {
        printf("DHT22 set pin high failed\n");
        return -1;
    }

    if (DHT22_ConfigPinMode(sdev, GPIO_CFG_INPUT) != 0) {
        printf("[ERR] Cannot set output mode before level setting\n");
        return -1;
    }

    int testLevel = DHT22_ReadPinLevel(sdev);
    printf("test level result : %d\n", testLevel);
    /* 3. 短暂延时后开始检测响应 */
    DHT22_DelayUs(30);
    
    return 0;
}

/**
 * @description: 等待DHT22响应信号 - 精确时序控制
 * @param sdev - 传感器设备指针
 * @return success : 0 error : -1
 */
static int DHT22_WaitResponse(struct SensorDevice *sdev)
{
    uint32_t timeout = DHT22_RESPONSE_TIMEOUT;
    int pin_val;
    
    /* 等待DHT22拉低总线（80us低电平响应信号） */
    while (timeout--) {
        pin_val = DHT22_ReadPinLevel(sdev);
        if (pin_val == GPIO_LOW) break;
        DHT22_DelayUs(1);
    }
    if (timeout == 0) {
        printf("DHT22 response timeout (wait low level)\n");
        return -1;
    }
    
    int testLevel = DHT22_ReadPinLevel(sdev);
    printf("waitRes test level result : %d , waste time : %d\n", testLevel, 1000-timeout);

    timeout = DHT22_RESPONSE_TIMEOUT;
    /* 等待DHT22拉高总线（80us高电平准备信号） */
    while (timeout--) {
        pin_val = DHT22_ReadPinLevel(sdev);
        if (pin_val == GPIO_HIGH) break;
        DHT22_DelayUs(1);
    }
    if (timeout == 0) {
        printf("DHT22 response timeout (wait high level)\n");
        return -1;
    }

    testLevel = DHT22_ReadPinLevel(sdev);
    printf("waitRes test level result : %d , waste time : %d\n", testLevel, 1000-timeout);
    
    return 0;
}

/**
 * @description: 读取一位数据 - 精确位时序解析
 * @param sdev - 传感器设备指针
 * @return 数据位值（0或1，0xFF表示错误）
 */
static uint8_t DHT22_ReadBit(struct SensorDevice *sdev)
{
    uint32_t high_duration = 0;
    uint32_t timeout = DHT22_RESPONSE_TIMEOUT;
    int pin_val;
    
    /* 等待50us低电平起始位 */
    while (timeout--) {
        pin_val = DHT22_ReadPinLevel(sdev);
        if (pin_val == GPIO_LOW) break;
        DHT22_DelayUs(1);
    }
    
    if (timeout == 0) {
        printf("DHT22 bit start timeout\n");
        return 0xFF;
    }
    
    /* 测量高电平持续时间 */
    timeout = DHT22_RESPONSE_TIMEOUT * 2;
    while (timeout--) {
        pin_val = DHT22_ReadPinLevel(sdev);
        if (pin_val == GPIO_HIGH) {
            high_duration++;
            DHT22_DelayUs(1);
        } else {
            break;
        }
    }
    
    printf("duration: %u\n", high_duration);
    /* 根据高电平持续时间判断数据位 - 符合说明书时序规范 */
    if (high_duration > DHT22_BIT_1_MIN_DURATION) {
        return 1;  /* 高电平持续时间长（>70us），表示1 */
    } else if (high_duration > 0) {
        return 0;  /* 高电平持续时间短（26-28us），表示0 */
    }
    
    printf("DHT22 bit read error, duration: %u\n", high_duration);
    return 0xFF;
}

/**
 * @description: 读取40位数据 - 完整数据帧接收
 * @param sdev - 传感器设备指针
 * @param data - 数据存储缓冲区
 * @return success : 0 error : -1
 */
static int DHT22_ReadData(struct SensorDevice *sdev, uint8_t *data)
{
    uint8_t i, j;
    uint8_t byte, bit_val;
    
    /* 读取5字节（40位）数据 */
    for (i = 0; i < 5; i++) {
        byte = 0;
        for (j = 0; j < 8; j++) {
            bit_val = DHT22_ReadBit(sdev);
            if (bit_val == 0xFF) {
                printf("DHT22 read bit error at byte %d bit %d\n", i, j);
                return -1;
            }
            byte = (byte << 1) | bit_val;
        }
        data[i] = byte;
        printf("DHT22 data byte[%d]: 0x%02X\n", i, byte);
    }
    
    return 0;
}

/**
 * @description: 校验数据完整性 - 8位校验和验证
 * @param data - 数据缓冲区
 * @return success : 0 error : -1
 */
static int DHT22_CheckData(uint8_t *data)
{
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) {
        printf("DHT22 checksum error: %02X != %02X\n", sum, data[4]);
        return -1;
    }
    printf("DHT22 checksum OK: %02X == %02X\n", sum, data[4]);
    return 0;
}

/**
 * @description: 检查DHT22硬件连接
 * @param sdev - 传感器设备指针
 * @return success : 0 error : -1
 */
static int DHT22_HardwareCheck(struct SensorDevice *sdev)
{
    /* 检查GPIO设备是否正常打开 */
    if (sdev->fd < 0) {
        printf("DHT22 device not properly opened\n");
        return -1;
    }
    
    printf("Testing DHT22 hardware on pin %d...\n", SENSOR_DEVICE_DHT22_GPIO_PIN);
    
    /* 测试高低电平设置 */
    int ret = DHT22_SetPinLevel(sdev, GPIO_HIGH);
    if (ret != 0) {
        printf("GPIO pin %d high level test failed\n", SENSOR_DEVICE_DHT22_GPIO_PIN);
        return -1;
    }
    
    ret = DHT22_SetPinLevel(sdev, GPIO_LOW);
    if (ret != 0) {
        printf("GPIO pin %d low level test failed\n", SENSOR_DEVICE_DHT22_GPIO_PIN);
        return -1;
    }
    
    printf("DHT22 hardware check passed\n");
    return 0;
}

/**
 * @description: DHT22通信诊断
 * @param sdev - 传感器设备指针
 * @return success : 0 error : -1
 */
// static int DHT22_CommunicationDiagnostic(struct SensorDevice *sdev)
// {
//     printf("=== DHT22 Communication Diagnostic ===\n");
    
//     /* 测试总线空闲状态 */
//     printf("1. Testing bus idle state...\n");
//     int idle_level = DHT22_ReadPinLevel(sdev);
//     printf("Bus idle level: %d (expected: 1)\n", idle_level);
    
//     if (idle_level != 1) {
//         printf("WARNING: Bus not at high level when idle\n");
//     }
    
//     /* 测试起始信号 */
//     printf("2. Testing start signal...\n");
//     if (DHT22_SendStartSignal(sdev) != 0) {
//         printf("FAIL: Start signal generation\n");
//         return -1;
//     }
//     printf("PASS: Start signal generated\n");
    
//     /* 测试响应检测 */
//     printf("3. Testing response detection...\n");
//     if (DHT22_EnhancedWaitResponse(sdev) != 0) {
//         printf("FAIL: No response from DHT22\n");
        
//         /* 检查总线状态 */
//         int bus_state = DHT22_ReadPinLevel(sdev);
//         printf("Current bus state: %d\n", bus_state);
//         return -1;
//     }
//     printf("PASS: DHT22 responded correctly\n");
    
//     printf("=== Communication Diagnostic COMPLETED ===\n");
//     return 0;
// }

/**
 * @description: 打开DHT22传感器设备
 * @param sdev - 传感器设备指针
 * @return success : 0 error : -1
 */
static int SensorDeviceOpen(struct SensorDevice *sdev)
{
    sdev->fd = PrivOpen(SENSOR_DEVICE_DHT22_DEV, O_RDWR);
    if (sdev->fd < 0) {
        printf("Open %s failed!\n", SENSOR_DEVICE_DHT22_DEV);
        return -1;
    }
    
    // printf("Device %s opened successfully, FD=%d\n", SENSOR_DEVICE_DHT22_DEV, sdev->fd);
    
    /* 上电后等待1秒越过不稳定状态 - 符合说明书要求 */
    printf("DHT22 power on, waiting 1s for stabilization...\n");
    PrivTaskDelay(1000);
    
    printf("DHT22 sensor opened successfully on pin %d\n", SENSOR_DEVICE_DHT22_GPIO_PIN);
    return 0;
}

/**
 * @description: 读取传感器数据 - 完整通信流程
 * @param sdev - 传感器设备指针
 * @param len - 读取数据长度
 * @return success: 0 , failure: -1
 */
static int SensorDeviceRead(struct SensorDevice *sdev, size_t len)
{
    int ret;
    uint8_t data[5] = {0};
    
    /* 硬件连接检查 */
    // ret = DHT22_HardwareCheck(sdev);
    // if (ret != 0) {
    //     printf("DHT22 hardware check failed\n");
    //     return -1;
    // }

    printf("Starting DHT22 communication protocol...\n");

    if(sdev->fd < 0){
        printf("open pin fd error:%d\n", sdev->fd);
        return -1;
    }
    
    /* 完整的DHT22通信流程 */
    ret = DHT22_SendStartSignal(sdev);
    if (ret != 0) {
        printf("DHT22 start signal failed\n");
        return -1;
    }
    
    ret = DHT22_WaitResponse(sdev);
    if (ret != 0) {
        printf("DHT22 response failed\n");
        return -1;
    }
    
    ret = DHT22_ReadData(sdev, data);
    if (ret != 0) {
        printf("DHT22 read data failed\n");
        return -1;
    }
    
    ret = DHT22_CheckData(data);
    if (ret != 0) {
        printf("DHT22 data check failed\n");
        return -1;
    }
    
    /* 存储数据到缓冲区 */
    if (len >= 5) {
        memcpy(sdev->buffer, data, 5);
    }
    
    // printf("DHT22 communication completed successfully\n");
    return 0;
}

/**
 * @description: 关闭传感器设备
 * @param sdev - 传感器设备指针
 * @return success: 0
 */
static int SensorDeviceClose(struct SensorDevice *sdev)
{
    if (sdev->fd >= 0) {
        printf("Closing DHT22 sensor...\n");
        PrivClose(sdev->fd);
        sdev->fd = -1;
    }
    return 0;
}

/* 传感器操作接口 */
static struct SensorDone done =
{
    .open = SensorDeviceOpen,
    .close = SensorDeviceClose,
    .read = SensorDeviceRead,
    .write = NULL,
    .ioctl = NULL
};

/**
 * @description: 初始化DHT22传感器并注册
 * @return void
 */
static void SensorDeviceDht22Init(void)
{
    dht22.name = SENSOR_DEVICE_DHT22;
    dht22.info = &info;
    dht22.done = &done;
    dht22.status = SENSOR_DEVICE_PASSIVE;

    SensorDeviceRegister(&dht22);
    printf("DHT22 sensor device registered\n");
}

static struct SensorQuantity dht22_temperature;

/**
 * @description: 解析DHT22温度数据 - 正确处理负温度
 * @param quant - 传感器量指针
 * @return 温度值（扩大10倍，单位：0.1℃）
 */
static int32_t ReadTemperature(struct SensorQuantity *quant)
{
    if (!quant || !quant->sdev) {
        printf("DHT22 temperature sensor invalid\n");
        return -1;
    }

    if (quant->sdev->done->read != NULL) {
        if (quant->sdev->status == SENSOR_DEVICE_PASSIVE) {
            /* 读取传感器数据 */
            if (quant->sdev->done->read(quant->sdev, 5) == 0) {
                uint16_t temp_raw;
                float temperature;
                
                /* 解析温度数据（16位） */
                temp_raw = (quant->sdev->buffer[2] << 8) | quant->sdev->buffer[3];
                
                /* 检查温度正负（最高位为符号位） */
                if (temp_raw & 0x8000) {
                    /* 负温度：清除符号位后计算 */
                    temp_raw &= 0x7FFF;
                    temperature = -(float)temp_raw * 0.1;
                    printf("DHT22 Temperature: -%.1f°C\n", temperature);
                } else {
                    /* 正温度 */
                    temperature = (float)temp_raw * 0.1;
                    printf("DHT22 Temperature: %.1f°C\n", temperature);
                }
                
                /* 返回扩大10倍的温度值（单位：0.1℃） */
                return (int32_t)(temperature * 10);
            } else {
                printf("DHT22 read data failed\n");
            }
        } else {
            printf("Please set DHT22 to passive mode\n");
        }
    } else {
        printf("DHT22 read function not available\n");
    }
    
    return -1;
}

/**
 * @description: 初始化DHT22温度量并注册
 * @return 0
 */
int Dht22TemperatureInit(void)
{
    SensorDeviceDht22Init();
    
    dht22_temperature.name = SENSOR_QUANTITY_DHT22_TEMPERATURE;
    dht22_temperature.type = SENSOR_QUANTITY_TEMP;
    dht22_temperature.value.decimal_places = 1;
    dht22_temperature.value.max_std = 800;    /* 80.0℃ */
    dht22_temperature.value.min_std = -400;   /* -40.0℃ */
    dht22_temperature.value.last_value = SENSOR_QUANTITY_VALUE_ERROR;
    dht22_temperature.value.max_value = SENSOR_QUANTITY_VALUE_ERROR;
    dht22_temperature.value.min_value = SENSOR_QUANTITY_VALUE_ERROR;
    dht22_temperature.sdev = &dht22;
    dht22_temperature.ReadValue = ReadTemperature;

    SensorQuantityRegister(&dht22_temperature);
    printf("DHT22 temperature quantity registered\n");

    return 0;
}