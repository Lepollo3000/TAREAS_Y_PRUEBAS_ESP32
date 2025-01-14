/*
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

static const char *TAG = "ADC_EXAMPLE";
static const char *TEMP_TAG = "DS18x20_TEST";
static esp_adc_cal_characteristics_t adc1_chars;

void app_main(void)
{
    uint32_t voltage;
    float ph;
    float lectura;
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC2_CHANNEL_4, ADC_ATTEN_DB_11));

    while (1) 
    {
        voltage = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC2_CHANNEL_4), &adc1_chars);
        lectura = adc1_get_raw(ADC2_CHANNEL_4);
        //ph = (lectura * 5 / 1024);
        ESP_LOGI(TAG, "ADC1_CHANNEL_6: %d mV", voltage);
        //ESP_LOGI(TAG, "LECTURA: %f mV", ph);
        ESP_LOGI(TAG, "LECTURA: %f mV", lectura);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
*/

#include <inttypes.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <esp_log.h>
#include <esp_err.h>
#include <ds18x20.h>

static const gpio_num_t SENSOR_GPIO = GPIO_NUM_13;
static const int MAX_SENSORS = 1;
static const int RESCAN_INTERVAL = 8;
static const uint32_t LOOP_DELAY_MS = 500;

static const char *TAG = "ds18x20_test";

void ds18x20_test(void *pvParameter)
{
    ds18x20_addr_t addrs[MAX_SENSORS];
    float temps[MAX_SENSORS];
    size_t sensor_count = 0;

    // There is no special initialization required before using the ds18x20
    // routines.  However, we make sure that the internal pull-up resistor is
    // enabled on the GPIO pin so that one can connect up a sensor without
    // needing an external pull-up (Note: The internal (~47k) pull-ups of the
    // ESP do appear to work, at least for simple setups (one or two sensors
    // connected with short leads), but do not technically meet the pull-up
    // requirements from the ds18x20 datasheet and may not always be reliable.
    // For a real application, a proper 4.7k external pull-up resistor is
    // recommended instead!)
    gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);

    esp_err_t res;
    while (1)
    {
        // Every RESCAN_INTERVAL samples, check to see if the sensors connected
        // to our bus have changed.
        res = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS, &sensor_count);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Sensors scan error %d (%s)", res, esp_err_to_name(res));
            continue;
        }

        if (!sensor_count)
        {
            ESP_LOGW(TAG, "No sensors detected!");
            continue;
        }

        ESP_LOGI(TAG, "%d sensors detected", sensor_count);

        // If there were more sensors found than we have space to handle,
        // just report the first MAX_SENSORS..
        if (sensor_count > MAX_SENSORS)
            sensor_count = MAX_SENSORS;

        // Do a number of temperature samples, and print the results.
        for (int i = 0; i < RESCAN_INTERVAL; i++)
        {
            ESP_LOGI(TAG, "Measuring...");

            res = ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Sensors read error %d (%s)", res, esp_err_to_name(res));
                continue;
            }

            for (int j = 0; j < sensor_count; j++)
            {
                float temp_c = temps[j];
                float temp_f = (temp_c * 1.8) + 32;
                // Float is used in printf(). You need non-default configuration in
                // sdkconfig for ESP8266, which is enabled by default for this
                // example. See sdkconfig.defaults.esp8266
                ESP_LOGI(TAG, "Sensor %08" PRIx32 "%08" PRIx32 " (%s) reports %.3f°C (%.3f°F)",
                        (uint32_t)(addrs[j] >> 32), (uint32_t)addrs[j],
                        (addrs[j] & 0xff) == DS18B20_FAMILY_ID ? "DS18B20" : "DS18S20",
                        temp_c, temp_f);
            }

            // Wait for a little bit between each sample (note that the
            // ds18x20_measure_and_read_multi operation already takes at
            // least 750ms to run, so this is on top of that delay).
            vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
        }
    }
}

void app_main()
{
    xTaskCreate(ds18x20_test, "ds18x20_test", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL);
}
