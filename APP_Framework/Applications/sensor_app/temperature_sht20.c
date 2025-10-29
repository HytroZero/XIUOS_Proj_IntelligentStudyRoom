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
 * @file temperature_sht20.c
 * @brief SHT20 temperature example
 * @version 1.0
 * @author AIIT XUOS Lab
 * @date 2025.10.29
 */

#include <sensor.h>

/**
 * @description: Read a temperature
 * @return 0
 */
void TempSht20(void)
{
    int i = 0;
    int32_t temperature;
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_SHT20_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    SensorQuantityOpen(temp);

    for (i = 0; i < 10; i ++) {
        temperature = SensorQuantityReadValue(temp);
        if (temperature > 0)
            printf("Temperature : %d.%d C\n", temperature/10, temperature%10);
        else
            printf("Temperature : %d.%d C\n", temperature/10, -temperature%10);

        PrivTaskDelay(500);
    }

    SensorQuantityClose(temp);
}

struct SensorQuantity *GetTempQuantity(void)
{
    struct SensorQuantity *temp = SensorQuantityFind(SENSOR_QUANTITY_SHT20_TEMPERATURE, SENSOR_QUANTITY_TEMP);
    SensorQuantityOpen(temp);
    return temp;
}