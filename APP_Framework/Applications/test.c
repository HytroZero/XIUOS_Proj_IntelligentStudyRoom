// 说明：这里面参考了大pdf p116/560页《基于 ARM 开发板的中断响应性能测试》

// TODO:
// ARM实验书GPIO实验的TestGpio指令
// 然后修改TestGpio，输出PIN_BUS_NAME
// 用在这个里面
// 传感器信号接入PC11上升沿触发中断
// 在中断处理里面添加数据传输服务

#include <stdio.h>
#include <string.h>
// #include <user_api.h>
#include <transform.h>

// sb start
extern int FrameworkInit();
extern void ApplicationOtaTaskInit(void);

#ifdef OTA_BY_PLATFORM
extern int OtaTask(void);
#endif

#ifdef APPLICATION_WEBSERVER
extern int webserver(void);
#endif
// sb end

/*********************GPIO define 全是从各种文件里偷来的宏*********************/
#define GPIO_LOW                         0x00
#define GPIO_HIGH                        0x01

#define GPIO_CFG_OUTPUT                  0x00
#define GPIO_CFG_INPUT                   0x01
#define GPIO_CFG_INPUT_PULLUP            0x02
#define GPIO_CFG_INPUT_PULLDOWN          0x03
#define GPIO_CFG_OUTPUT_OD               0x04

#define GPIO_IRQ_EDGE_RISING             0x00
#define GPIO_IRQ_EDGE_FALLING            0x01
#define GPIO_IRQ_EDGE_BOTH               0x02
#define GPIO_IRQ_LEVEL_HIGH              0x03
#define GPIO_IRQ_LEVEL_LOW               0x04

#define GPIO_CONFIG_MODE                 0xffffffff
#define GPIO_IRQ_REGISTER                0xfffffffe
#define GPIO_IRQ_FREE                    0xfffffffd
#define GPIO_IRQ_DISABLE                 0xfffffffc
#define GPIO_IRQ_ENABLE                  0xfffffffb
#define GPIO_C11 140

#define PIN_BUS_NAME "pin" // 似乎确实是pin，但是无法BusFind()

static BusType pin; 

void Init() {
    pin = BusFind(PIN_BUS_NAME);
}

void PinIrqIsr(void *args) { // 中断处理函数
    printf("Fuck You\n");
}

int Configure_PC11_RisingEdgeIrq()
{
    // 仅需为PC11引脚声明配置相关的结构体
    struct PinParam sensorPin;
    struct BusConfigureInfo busConfig;
    int ret = 0;

    // 初始化总线配置结构体
    busConfig.configure_cmd = OPE_CFG;
    busConfig.private_data = (void *)&sensorPin;

    // --- 步骤 1: 将 PC11 设置为输入模式 ---
    sensorPin.cmd = GPIO_CONFIG_MODE;
    sensorPin.pin = GPIO_C11;
    sensorPin.mode = GPIO_CFG_INPUT;

    ret = BusDrvConfigure(pin->owner_driver, &busConfig);
    if (ret != EOK) {
        KPrintf("Failed to configure PC11 as input! Error code: %d\n", ret);
        return -ERROR;
    }

    // --- 步骤 2: 为 PC11 注册中断服务函数，并设置为上升沿触发 ---
    sensorPin.cmd = GPIO_IRQ_REGISTER;
    sensorPin.pin = GPIO_C11;
    // [MODIFIED] 将触发模式从 GPIO_IRQ_EDGE_BOTH 改为 GPIO_IRQ_EDGE_RISING
    sensorPin.irq_set.irq_mode = GPIO_IRQ_EDGE_RISING; 
    sensorPin.irq_set.hdr = (void(*)(void *))PinIrqIsr; // 回调函数指针保持不变
    sensorPin.irq_set.args = NONE;

    ret = BusDrvConfigure(pin->owner_driver, &busConfig);
    if (ret != EOK) {
        KPrintf("Failed to register IRQ for PC11! Error code: %d\n", ret);
        return -ERROR;
    }

    // --- 步骤 3: 使能 PC11 的外部中断 ---
    sensorPin.cmd = GPIO_IRQ_ENABLE;
    sensorPin.pin = GPIO_C11;

    ret = BusDrvConfigure(pin->owner_driver, &busConfig);
    if (ret != EOK) {
        KPrintf("Failed to enable IRQ for PC11! Error code: %d\n", ret);
        return -ERROR;
    }
    
    KPrintf("PC11 rising edge interrupt enabled successfully.\n");
    return 0;
}

int main(void)
{

    printf("\nHello, world!\n");

// sb start
    FrameworkInit();
    Init();
    Configure_PC11_RisingEdgeIrq();
#ifdef APPLICATION_OTA
    ApplicationOtaTaskInit();
#endif

#ifdef OTA_BY_PLATFORM
    OtaTask();
#endif

#ifdef APPLICATION_WEBSERVER
    webserver();
#endif

    return 0;
}
// int cppmain(void);


