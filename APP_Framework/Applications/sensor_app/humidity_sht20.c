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
 * @file humidity_sht20.c
 * @brief sht20 humidity example
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2025.10.29
 */

#include <sensor.h>

/**
 * @description: Read a himidity
 * @return 0
 */
void HumiSht20(void)
{
    int i = 0;
    int32_t humidity;
    struct SensorQuantity *humi = SensorQuantityFind(SENSOR_QUANTITY_SHT20_HUMIDITY, SENSOR_QUANTITY_HUMI);
    SensorQuantityOpen(humi);
    // for (i = 0; i < 10; i ++) {
        humidity = SensorQuantityReadValue(humi);
        printf("Humidity : %d.%d %%RH\n", humidity/10, humidity%10);
        // PrivTaskDelay(500);
    // }
    SensorQuantityClose(humi);
}

struct SensorQuantity* GetHumiQuantity(void)
{
    struct SensorQuantity *humi = SensorQuantityFind(SENSOR_QUANTITY_SHT20_HUMIDITY, SENSOR_QUANTITY_HUMI);
    SensorQuantityOpen(humi);
    return humi;
}