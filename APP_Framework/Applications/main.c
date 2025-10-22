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

#include <stdio.h>
#include <string.h>
// #include <user_api.h>
#include <transform.h>
#include <adapter_wifi.h>
#include <adapter.h>         
// #include "sensor_app/motion_hcsr501.c"

#define WIFI_SSID       "Factory"      
#define WIFI_PASSWORD   "00000000" 

extern int FrameworkInit();
extern void ApplicationOtaTaskInit(void);
extern void MotionHcSr501(void);
extern void TempDht22(void);

int WifiInitAndConnect(void)
{
    int ret = 0;
    
    // 1. 查找Wi-Fi适配器
    struct Adapter* adapter = AdapterDeviceFindByName(ADAPTER_WIFI_NAME);
    if (!adapter) {
        printf("Wi-Fi adapter not found!\n");
        return -1;
    }
    
    // 2. 打开Wi-Fi设备
    ret = AdapterDeviceOpen(adapter);
    if (ret != 0) {
        printf("Failed to open Wi-Fi device! Error: %d\n", ret);
        return ret;
    }
    printf("Wi-Fi device opened successfully.\n");
    
    // 3. 配置Wi-Fi连接参数（使用宏定义）
    static struct WifiParam param;
    memset(&param, 0, sizeof(struct WifiParam));
    strncpy((char *)param.wifi_ssid, WIFI_SSID, sizeof(param.wifi_ssid) - 1);
    strncpy((char *)param.wifi_pwd, WIFI_PASSWORD, sizeof(param.wifi_pwd) - 1);
    
    // 确保字符串以null结尾
    param.wifi_ssid[sizeof(param.wifi_ssid) - 1] = '\0';
    param.wifi_pwd[sizeof(param.wifi_pwd) - 1] = '\0';
    
    adapter->adapter_param = &param;
    
    // 4. 执行连接操作
    printf("Connecting to Wi-Fi: %s...\n", WIFI_SSID);
    ret = AdapterDeviceSetUp(adapter);
    if (ret != 0) {
        printf("Wi-Fi connection failed! Error: %d\n", ret);
        // 关闭设备以防资源泄漏
        AdapterDeviceClose(adapter);
        return ret;
    }
    
    printf("Wi-Fi connected successfully to: %s\n", WIFI_SSID);
    
    // 5. 可选：等待一段时间确保连接稳定
    PrivTaskDelay(3000);
    
    return ret;
}

#ifdef OTA_BY_PLATFORM
extern int OtaTask(void);
#endif

#ifdef APPLICATION_WEBSERVER
extern int webserver(void);
#endif



int main(void)
{
    printf("\nHello, world!\n");
    FrameworkInit();
#ifdef APPLICATION_OTA
    ApplicationOtaTaskInit();
#endif

#ifdef OTA_BY_PLATFORM
    OtaTask();
#endif

#ifdef APPLICATION_WEBSERVER
    webserver();
#endif
    

    // WifiInitAndConnect();
    // MotionHcSr501();
    TempDht22();
    return 0;
}
// int cppmain(void);


