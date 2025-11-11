#include <sensor.h>

#define SHT20_CMD_SOFT_RESET          0xFE
#define SHT20_CMD_TEMP_HOLD           0xE3

static struct SensorDevice sht20;

static struct SensorProductInfo info = {
    (SENSOR_ABILITY_HUMI | SENSOR_ABILITY_TEMP),
    "Sensirion",
    "SHT20",
};

/**
 * @description: 打开SHT20传感器设备
 * @param sdev - 传感器设备指针
 * @return 成功: 0 错误: -1
 */
static int SensorDeviceOpen(struct SensorDevice *sdev) {
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
 * @description: 读取传感器数据
 * @param sdev - 传感器设备指针
 * @param len - 读取数据长度
 * @return 成功: 0, 失败: -1
 */
static int SensorDeviceRead(struct SensorDevice *sdev, size_t len) {
    if (PrivRead(sdev->fd, sdev->buffer, len) < 0)
        return -1;
    return 0;
}

/**
 * @description: 写入传感器命令
 * @param sdev - 传感器设备指针
 * @param buf - 写入数据缓冲区
 * @param len - 写入数据长度
 * @return 成功: 0, 失败: -1
 */
static int SensorDeviceWrite(struct SensorDevice *sdev, const void *buf, size_t len) {
    if (PrivWrite(sdev->fd, buf, len) < 0)
        return -1;
    return 0;
}

/**
 * @description: 软复位SHT20传感器
 * @param sdev - 传感器设备指针
 * @return 成功: 0, 失败: -1
 */
// static int SensorDeviceReset(struct SensorDevice *sdev) {
//     uint8_t reset_cmd = SHT20_CMD_SOFT_RESET;
    
//     if (sdev->done->write(sdev, &reset_cmd, 1) < 0)
//         return -1;
    
//     PrivTaskDelay(15); // 等待复位完成
//     return 0;
// }

static struct SensorDone done = {
    SensorDeviceOpen,
    NULL,
    SensorDeviceRead,
    SensorDeviceWrite,
    NULL,
};

/**
 * @description: 初始化SHT20传感器并注册
 */
static void SensorDeviceSht20Init(void) {
    sht20.name = SENSOR_DEVICE_SHT20;
    sht20.info = &info;
    sht20.done = &done;
    sht20.status = SENSOR_DEVICE_PASSIVE;

    SensorDeviceRegister(&sht20);
}

static struct SensorQuantity sht20_temperature;

/**
 * @description: 温度信号转换为实际温度值
 * @param raw_data - 原始温度数据
 * @return 温度值(°C × 10)
 */
static float Sht20ConvertTemperature(uint16_t raw_data) {
    // 清除状态位(最后两位)
    raw_data &= 0xFFFC;
    
    // 根据数据手册公式转换: T = -46.85 + 175.72 × S_T / 2^16
    float temperature = -46.85 + 175.72 * (raw_data / 65536.0);
    
    return temperature;
}

/**
 * @description: 读取SHT20温度值
 * @param quant - 传感器量指针
 * @return 温度值(°C × 10)
 */
static int32_t ReadTemperature(struct SensorQuantity *quant) {
    if (!quant)
        return -1;

    uint8_t temp_cmd = SHT20_CMD_TEMP_HOLD;
    float result;
    
    if (quant->sdev->done->read != NULL && quant->sdev->done->write != NULL) {
        if (quant->sdev->status == SENSOR_DEVICE_PASSIVE) {
            // 发送温度测量命令
            if (quant->sdev->done->write(quant->sdev, &temp_cmd, 1) < 0) {
                printf("Send temperature command failed\n");
                return -1;
            }
            
            // 等待测量完成(最大66ms)
            PrivTaskDelay(70);
            
            // 读取3字节数据(2字节数据 + 1字节CRC)
            if (quant->sdev->done->read(quant->sdev, 3) == 0) {
                // 组合温度数据
                uint16_t raw_temp = (quant->sdev->buffer[0] << 8) | quant->sdev->buffer[1];
                
                // 转换温度值
                result = Sht20ConvertTemperature(raw_temp);
                
                // 返回温度值(放大10倍)
                return (int32_t)(result * 10);
            }
        } else {
            printf("Please set passive mode.\n");
        }
    } else {
        printf("%s don't have read/write done.\n", quant->name);
    }
    
    return -1;
}


/**
 * @description: 初始化SHT20温度量程并注册
 * @return 0
 */
int Sht20TemperatureInit(void) {
    SensorDeviceSht20Init();
    
    sht20_temperature.name = SENSOR_QUANTITY_SHT20_TEMPERATURE;
    sht20_temperature.type = SENSOR_QUANTITY_TEMP;
    sht20_temperature.value.decimal_places = 1;
    sht20_temperature.value.max_std = 850;   // -40°C to +125°C
    sht20_temperature.value.min_std = -400;  // -40.0°C
    sht20_temperature.value.last_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_temperature.value.max_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_temperature.value.min_value = SENSOR_QUANTITY_VALUE_ERROR;
    sht20_temperature.sdev = &sht20;
    sht20_temperature.ReadValue = ReadTemperature;

    SensorQuantityRegister(&sht20_temperature);

    return 0;
}