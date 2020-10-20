#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"
#include "logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt-sn.h"
#include "commit.h"
#include "hpm.h"
#include "hdc.h"
#include "adc.h"
#include "tsl.h"
#include "socomec.h"
#include "volume.h"
#include "config.h"

#define SAMPLE_RATE_MS 1000
#define TIME_OFFSET (SAMPLE_RATE_MS / portTICK_PERIOD_MS)

void measurements_task(void *pvParameters) {
    adc_setup();
    tsl_setup();
    volume_setup();
    smart_meter_setup();

    // Take a few samples from various sensors;
    // This will stagger the times when they need to calculate averages etc
    sound_query();
    sound_query();
    sound_query();
    sound_query();

    hdc_query();
    hdc_query();
    hdc_query();

    hpm_query();
    hpm_query();
    smart_meter_query();
    smart_meter_query();

    tsl_query();

    for(;;) {
        TickType_t before = xTaskGetTickCount();

        hpm_query();   // smog sensor
        hdc_query();   // humidity and temperature
        sound_query(); // sound
        tsl_query();   // light
        smart_meter_query();
        query_pulsecount();
        battery_query();

        TickType_t after = xTaskGetTickCount();
        TickType_t delay = (before + TIME_OFFSET) - after;

        if(before + TIME_OFFSET > after) {
            vTaskDelay(delay);
        }
    }
}

