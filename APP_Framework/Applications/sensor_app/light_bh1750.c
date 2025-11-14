#include <sensor.h>

/**
 * @description: Read a light intensity
 * @return 0
 */
void LightBh1750(void)
{
    int i = 0;
    int32_t light_intensity;
    struct SensorQuantity *light = SensorQuantityFind(SENSOR_QUANTITY_BH1750_LIGHT, SENSOR_QUANTITY_LIGHT);
    SensorQuantityOpen(light);

    for (i = 0; i < 10; i ++) {
        light_intensity = SensorQuantityReadValue(light);
        if (light_intensity >= 0)
            printf("Light Intensity : %d.%d lx\n", light_intensity/10, light_intensity%10);
        else
            printf("Read light intensity failed: %d\n", light_intensity);

        PrivTaskDelay(500);
    }

    SensorQuantityClose(light);
}

/**
 * @description: Get light quantity handle
 * @return light sensor quantity pointer
 */
struct SensorQuantity *GetLightQuantity(void)
{
    struct SensorQuantity *light = SensorQuantityFind(SENSOR_QUANTITY_BH1750_LIGHT, SENSOR_QUANTITY_LIGHT);
    SensorQuantityOpen(light);
    return light;
}

/**
 * @description: Continuous light monitoring with custom interval
 * @param count - number of readings to take
 * @param interval_ms - interval between readings in milliseconds
 */
void ContinuousLightMonitor(int count, uint32_t interval_ms)
{
    int32_t light_intensity;
    struct SensorQuantity *light = GetLightQuantity();
    
    if (!light) {
        printf("Failed to get light sensor quantity\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        light_intensity = SensorQuantityReadValue(light);
        if (light_intensity >= 0) {
            printf("[%d/%d] Light: %d.%d lx\n", 
                   i + 1, count, 
                   light_intensity / 10, 
                   light_intensity % 10);
        } else {
            printf("[%d/%d] Read failed: %d\n", i + 1, count, light_intensity);
        }
        
        PrivTaskDelay(interval_ms);
    }

    SensorQuantityClose(light);
}

/**
 * @description: Read single light intensity value
 * @return light intensity in lx * 10, or negative value if error
 */
int32_t ReadSingleLightValue(void)
{
    int32_t light_intensity;
    struct SensorQuantity *light = GetLightQuantity();
    
    if (!light) {
        printf("Failed to get light sensor quantity\n");
        return -1;
    }

    light_intensity = SensorQuantityReadValue(light);
    SensorQuantityClose(light);
    
    return light_intensity;
}

/**
 * @description: Example of light level classification
 */
void LightLevelClassification(void)
{
    int32_t light_intensity = ReadSingleLightValue();
    
    if (light_intensity < 0) {
        printf("Failed to read light intensity\n");
        return;
    }
    
    float light_lx = light_intensity / 10.0f;
    
    printf("Current light: %.1f lx - ", light_lx);
    
    if (light_lx < 10) {
        printf("Dark\n");
    } else if (light_lx < 100) {
        printf("Dim\n");
    } else if (light_lx < 1000) {
        printf("Normal\n");
    } else if (light_lx < 10000) {
        printf("Bright\n");
    } else {
        printf("Very Bright\n");
    }
}