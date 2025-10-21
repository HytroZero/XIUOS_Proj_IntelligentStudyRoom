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
 * @file motion_hcsr501.c
 * @brief HC-SR501 motion detection example
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2024.06.20
 */

#include <sensor.h>
// #include <task.h>

/**
 * @description: Detect motion state
 * @return 0
 */
void MotionHcSr501(void)
{
    int i = 0;
    int32_t motion_state;
    
    /* 查找运动检测传感器量 */
    struct SensorQuantity *motion = SensorQuantityFind(SENSOR_QUANTITY_HCSR501_MOTION, SENSOR_QUANTITY_MOTION);
    if (!motion) {
        printf("Cannot find HC-SR501 motion quantity!\n");
        return;
    }
    
    /* 打开传感器量 */
    SensorQuantityOpen(motion);
    
    /* 连续读取10次运动状态 */
    for (i = 0; i < 10; i++) {
        motion_state = SensorQuantityReadValue(motion);
        
        /* 打印运动状态 */
        printf("Motion state: %s\n", motion_state ? "DETECTED" : "IDLE");
        
        /* 延迟500ms */
        PrivTaskDelay(500);
    }
    
    /* 关闭传感器量 */
    SensorQuantityClose(motion);
}

#ifdef USING_SHELL
#include <shell.h>

/**
 * @description: Shell command to test HC-SR501
 * @param argc - argument count
 * @param argv - argument vector
 * @return 0
 */
int HcSr501Test(int argc, char *argv[])
{
    MotionHcSr501();
    return 0;
}
PRIV_SHELL_CMD_FUNCTION(HcSr501Test, Test HC-SR501 motion sensor, PRIV_SHELL_CMD_MAIN_ATTR);

#endif /* USING_SHELL */