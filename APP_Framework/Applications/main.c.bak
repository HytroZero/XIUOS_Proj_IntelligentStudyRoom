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
// #include <adapter_wifi.h>
// #include <adapter.h>         
// #include "sensor_app/motion_hcsr501.c"

extern int FrameworkInit();
extern void ApplicationOtaTaskInit(void);



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
    

    WifiInitAndConnect();
    // TempSht20();
    // HumiSht20();
    /* 6. 创建并行传感器任务 */

    printf("7. Creating parallel sensor tasks...\n");
    if (CreateAndStartSensorTasks() < 0) {
        printf(" Failed to create sensor tasks\n");
        return -1;
    }
    printf(" Parallel tasks created and started\n");
    
    // /* 7. 主任务监控 */
    // printf("\n"
    //        "===============================================\n"
    //        "        Starting Parallel Sensor Monitoring     \n"
    //        "===============================================\n\n");
    
    // MonitorSensorTasks();
    
    /* 8. 清理资源 */
    // printf("\n"
    //        "===============================================\n"
    //        "          System Shutdown Sequence            \n"
    //        "===============================================\n\n");
    
    // StopSensorTasks();
    
    return 0;
}
// int cppmain(void);

