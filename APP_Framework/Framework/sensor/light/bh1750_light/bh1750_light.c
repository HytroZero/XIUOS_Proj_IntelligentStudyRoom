#include <sensor.h>

#define BH1750_CMD_POWER_ON        0x01
#define BH1750_CMD_RESET           0x07
#define BH1750_CMD_MEASUREMENT     0x20

static struct SensorDevice bh1750;

static struct SensorProductInfo info = {
    SENSOR_ABILITY_LIGHT,  // 光照传感器能力
    "ROHM",                // 制造商
    "BH1750",              // 型号
};

/**
 * @description: 打开BH1750传感器设备
 * @param sdev - 传感器设备指针
 * @return 成功: 0 错误: -1
 */
static int SensorDeviceOpen(struct SensorDevice *sdev) {
    int result;
    uint16_t i2c_dev_addr = SENSOR_DEVICE_BH1750_I2C_ADDR;
    
    sdev->fd = PrivOpen(SENSOR_DEVICE_BH1750_DEV, O_RDWR);
    if (sdev->fd < 0) {
        printf("open %s error\n", SENSOR_DEVICE_BH1750_DEV);
        return -1;
    }

#ifdef ADD_XIZI_FEATURES
    struct PrivIoctlCfg ioctl_cfg;
    ioctl_cfg.ioctl_driver_type = I2C_TYPE;
    ioctl_cfg.args = &i2c_dev_addr;
    result = PrivIoctl(sdev->fd, OPE_INT, &ioctl_cfg);
#else
    result = 0; // 非XIZI环境可能需要其他初始化方式
#endif

    return result;
}

/**
 * @description: 关闭BH1750传感器设备
 * @param sdev - 传感器设备指针
 * @return 成功: 0
 */
static int SensorDeviceClose(struct SensorDevice *sdev) {
    if (sdev->fd >= 0) {
        PrivClose(sdev->fd);
        sdev->fd = -1;
    }
    return 0;
}

/**
 * @description: 读取传感器数据
 * @param sdev - 传感器设备指针
 * @param len - 读取数据长度
 * @return 成功: 0, 失败: -1
 */
static int SensorDeviceRead(struct SensorDevice *sdev, size_t len) {
    if (PrivRead(sdev->fd, sdev->buffer, len) < 0) {
        printf("BH1750 read error\n");
        return -1;
    }
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
    if (PrivWrite(sdev->fd, buf, len) < 0) {
        printf("BH1750 write error\n");
        return -1;
    }
    return 0;
}

/**
 * @description: 复位BH1750传感器
 * @param sdev - 传感器设备指针
 * @return 成功: 0, 失败: -1
 */
static int SensorDeviceReset(struct SensorDevice *sdev) {
    uint8_t reset_cmd = BH1750_CMD_RESET;
    
    if (sdev->done->write(sdev, &reset_cmd, 1) < 0) {
        printf("BH1750 reset failed\n");
        return -1;
    }
    
    PrivTaskDelay(5); // 等待复位完成
    return 0;
}

/**
 * @description: 上电BH1750传感器
 * @param sdev - 传感器设备指针
 * @return 成功: 0, 失败: -1
 */
static int SensorDevicePowerOn(struct SensorDevice *sdev) {
    uint8_t power_on_cmd = BH1750_CMD_POWER_ON;
    
    if (sdev->done->write(sdev, &power_on_cmd, 1) < 0) {
        printf("BH1750 power on failed\n");
        return -1;
    }
    
    PrivTaskDelay(5); // 等待上电完成
    return 0;
}

static struct SensorDone done = {
    SensorDeviceOpen,
    SensorDeviceClose,
    SensorDeviceRead,
    SensorDeviceWrite,
    SensorDeviceReset,
};

/**
 * @description: 初始化BH1750传感器并注册
 */
static void SensorDeviceBh1750Init(void) {
    bh1750.name = SENSOR_DEVICE_BH1750;
    bh1750.info = &info;
    bh1750.done = &done;
    bh1750.status = SENSOR_DEVICE_PASSIVE;

    SensorDeviceRegister(&bh1750);
}

static struct SensorQuantity bh1750_light;

/**
 * @description: 光照强度信号转换为实际值
 * @param raw_data - 原始光照数据
 * @return 光照强度值(lx)
 */
static float Bh1750ConvertLight(uint16_t raw_data) {
    // BH1750转换公式: 光照强度(lx) = 原始值 / 1.2
    // 高分辨率模式下，分辨率=1 lx/count
    float light_intensity = (float)raw_data / 1.2f;
    return light_intensity;
}

/**
 * @description: 读取BH1750光照强度值
 * @param quant - 传感器量指针
 * @return 光照强度值(lx × 10)
 */
static int32_t ReadLightIntensity(struct SensorQuantity *quant) {
    if (!quant) {
        return -1;
    }

    uint8_t measure_cmd = BH1750_CMD_MEASUREMENT;
    float result;
    
    if (quant->sdev->done->read != NULL && quant->sdev->done->write != NULL) {
        if (quant->sdev->status == SENSOR_DEVICE_PASSIVE) {
            // 发送测量命令
            if (quant->sdev->done->write(quant->sdev, &measure_cmd, 1) < 0) {
                printf("Send BH1750 measurement command failed\n");
                return -1;
            }
            
            // 等待测量完成（高分辨率模式最大180ms）
            PrivTaskDelay(200);
            
            // 读取2字节数据
            if (quant->sdev->done->read(quant->sdev, 2) == 0) {
                // 组合光照数据
                uint16_t raw_light = (quant->sdev->buffer[0] << 8) | quant->sdev->buffer[1];
                
                // 转换光照强度值
                result = Bh1750ConvertLight(raw_light);
                
                // 返回光照强度值(放大10倍保持精度)
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
 * @description: 初始化BH1750光照量程并注册
 * @return 0
 */
int Bh1750LightInit(void) {
    SensorDeviceBh1750Init();
    
    bh1750_light.name = SENSOR_QUANTITY_BH1750_LIGHT;
    bh1750_light.type = SENSOR_QUANTITY_LIGHT;
    bh1750_light.value.decimal_places = 1;        // 小数点后1位
    bh1750_light.value.max_std = 655350;          // 最大光照强度 65535 lx
    bh1750_light.value.min_std = 0;               // 最小光照强度 0 lx
    bh1750_light.value.last_value = SENSOR_QUANTITY_VALUE_ERROR;
    bh1750_light.value.max_value = SENSOR_QUANTITY_VALUE_ERROR;
    bh1750_light.value.min_value = SENSOR_QUANTITY_VALUE_ERROR;
    bh1750_light.sdev = &bh1750;
    bh1750_light.ReadValue = ReadLightIntensity;

    SensorQuantityRegister(&bh1750_light);

    return 0;
}