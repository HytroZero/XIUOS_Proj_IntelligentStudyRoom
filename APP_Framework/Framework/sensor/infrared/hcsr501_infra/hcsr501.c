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

#include <sensor.h>
#include <device.h>
#include <transform.h>
#include <stdio.h>
#include <string.h>
#include <board.h>  // 包含Kconfig生成的配置头文件

/********************* 配置宏定义（从Kconfig获取） *********************/
#define HC_SR501_DEVICE_NAME      CONFIG_SENSOR_DEVICE_HCSR501
#define HC_SR501_GPIO_DEV         CONFIG_SENSOR_DEVICE_HCSR501_GPIO_DEV
#define HC_SR501_GPIO_PIN         CONFIG_SENSOR_DEVICE_HCSR501_GPIO_PIN
#define HC_SR501_IRQ_MODE         CONFIG_SENSOR_DEVICE_HCSR501_IRQ_MODE

/********************* 全局变量 *********************/
static struct SensorDevice hc_sr501;
static BusType gpio_bus;

/********************* 传感器信息 *********************/
static struct SensorProductInfo info = {
    SENSOR_ABILITY_MOTION,    // 运动检测能力
    "HC",                     // 厂商
    "SR501",                  // 型号
};

/********************* 中断处理函数 *********************/
static void HcSr501IrqHandler(void *args)
{
    // 读取当前GPIO状态
    struct PinStat pin_stat;
    pin_stat.pin = HC_SR501_GPIO_PIN;
    
    struct PrivIoctlCfg ioctl_cfg = {
        .ioctl_driver_type = PIN_TYPE,
        .args = &pin_stat
    };
    
    if (PrivIoctl(hc_sr501.fd, OPE_INT, &ioctl_cfg) == EOK) {
        // 将状态保存到缓冲区
        hc_sr501.buffer[0] = pin_stat.val;
        
        // 触发数据处理（可通过消息队列通知其他任务）
        printf("[HC-SR501] Motion %s detected!\n", 
               pin_stat.val ? "START" : "END");
    }
}

/********************* 设备操作函数 *********************/
static int SensorDeviceOpen(struct SensorDevice *sdev)
{
    // 1. 打开GPIO设备
    sdev->fd = PrivOpen(HC_SR501_GPIO_DEV, O_RDWR);
    if (sdev->fd < 0) {
        printf("Open %s failed!\n", HC_SR501_GPIO_DEV);
        return -1;
    }

    // 2. 配置GPIO为输入模式
    struct PinParam pin_cfg = {
        .cmd = GPIO_CONFIG_MODE,
        .pin = HC_SR501_GPIO_PIN,
        .mode = GPIO_CFG_INPUT
    };

    struct PrivIoctlCfg ioctl_cfg = {
        .ioctl_driver_type = PIN_TYPE,
        .args = &pin_cfg
    };

    if (PrivIoctl(sdev->fd, OPE_CFG, &ioctl_cfg) != EOK) {
        printf("Configure GPIO pin %d failed!\n", HC_SR501_GPIO_PIN);
        PrivClose(sdev->fd);
        return -1;
    }

    // 3. 注册中断处理函数
    pin_cfg.cmd = GPIO_IRQ_REGISTER;
    pin_cfg.irq_set.irq_mode = HC_SR501_IRQ_MODE;
    pin_cfg.irq_set.hdr = HcSr501IrqHandler;
    pin_cfg.irq_set.args = NULL;

    if (PrivIoctl(sdev->fd, OPE_CFG, &ioctl_cfg) != EOK) {
        printf("Register IRQ for pin %d failed!\n", HC_SR501_GPIO_PIN);
        PrivClose(sdev->fd);
        return -1;
    }

    // 4. 使能中断
    pin_cfg.cmd = GPIO_IRQ_ENABLE;
    if (PrivIoctl(sdev->fd, OPE_CFG, &ioctl_cfg) != EOK) {
        printf("Enable IRQ for pin %d failed!\n", HC_SR501_GPIO_PIN);
        PrivClose(sdev->fd);
        return -1;
    }

    printf("HC-SR501 initialized on pin %d\n", HC_SR501_GPIO_PIN);
    return 0;
}

static int SensorDeviceRead(struct SensorDevice *sdev, size_t len)
{
    // 直接从缓冲区获取最新状态（由中断更新）
    if (len < sizeof(uint8_t)) {
        return -1;
    }
    return 0; // 数据已在中断中更新
}

static int SensorDeviceClose(struct SensorDevice *sdev)
{
    if (sdev->fd >= 0) {
        // 禁用中断
        struct PinParam pin_cfg = {
            .cmd = GPIO_IRQ_DISABLE,
            .pin = HC_SR501_GPIO_PIN
        };
        
        struct PrivIoctlCfg ioctl_cfg = {
            .ioctl_driver_type = PIN_TYPE,
            .args = &pin_cfg
        };
        
        PrivIoctl(sdev->fd, OPE_CFG, &ioctl_cfg);
        PrivClose(sdev->fd);
        sdev->fd = -1;
    }
    return 0;
}

/********************* 传感器操作接口 *********************/
static struct SensorDone done = {
    .open = SensorDeviceOpen,
    .close = SensorDeviceClose,
    .read = SensorDeviceRead,
    .write = NULL,
    .ioctl = NULL
};

/********************* 传感器量定义 *********************/
static struct SensorQuantity hc_sr501_motion;

static int32_t ReadMotion(struct SensorQuantity *quant)
{
    if (!quant || !quant->sdev) return -1;
    
    // 直接返回缓冲区中的最新状态
    return (int32_t)quant->sdev->buffer[0];
}

/********************* 初始化函数 *********************/
void SensorDeviceHcSr501Init(void)
{
    // 初始化传感器设备
    hc_sr501.name = HC_SR501_DEVICE_NAME;
    hc_sr501.info = &info;
    hc_sr501.done = &done;
    hc_sr501.status = SENSOR_DEVICE_ACTIVE;
    
    // 注册传感器
    SensorDeviceRegister(&hc_sr501);
    
    // 初始化运动检测量
    hc_sr501_motion.name = SENSOR_QUANTITY_HCSR501_MOTION;
    hc_sr501_motion.type = SENSOR_QUANTITY_MOTION;
    hc_sr501_motion.value.decimal_places = 0;
    hc_sr501_motion.value.max_std = 1;
    hc_sr501_motion.value.min_std = 0;
    hc_sr501_motion.sdev = &hc_sr501;
    hc_sr501_motion.ReadValue = ReadMotion;
    
    // 注册传感器量
    SensorQuantityRegister(&hc_sr501_motion);
}

/********************* Shell测试接口 *********************/
#ifdef USING_SHELL
#include <shell.h>

int HcSr501Test(int argc, char *argv[])
{
    int32_t motion = ReadMotion(&hc_sr501_motion);
    printf("Current motion state: %s\n", motion ? "DETECTED" : "IDLE");
    return 0;
}
PRIV_SHELL_CMD_FUNCTION(HcSr501Test, Test HC-SR501 sensor, PRIV_SHELL_CMD_MAIN_ATTR);

#endif