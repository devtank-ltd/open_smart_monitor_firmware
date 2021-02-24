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
#include "ds18b20.h"
#include "stats.h"

#define SAMPLE_RATE_MS 1000
#define TIME_OFFSET pdMS_TO_TICKS(SAMPLE_RATE_MS)
#define pdTICKS_TO_SECS(xTicks) ((xTicks) / (float)configTICKRATEHZ)

void measurements_task(void *pvParameters) {
    adc_setup();
    tsl_setup();
    volume_setup();
    smart_meter_setup();
    ds18b20_init();
    hpm_setup();

    TickType_t next_due[ARRAY_SIZE(parameter_names)];
    for(int i = 0; i < ARRAY_SIZE(parameter_names); i++)
        next_due[i] = xTaskGetTickCount();

    for(;;) {
        TickType_t before = xTaskGetTickCount();
        DEBUG_PRINTF("before %u", before);
        
        for(int i = 0; i < ARRAY_SIZE(parameter_names); i++) {
            if(next_due[i] < before) {
                int samplerate = get_sample_rate(i);
                if(!samplerate) continue;
                DEBUG_PRINTF("%s timedelta %d, samplerate %d", parameter_names[i], get_timedelta(i), samplerate);
                parameter_getters[i]();
                TickType_t offs = pdMS_TO_TICKS(get_timedelta(i) * 60 * 1000);
                next_due[i] = before + offs;
                DEBUG_PRINTF("next_due[i] = %u", next_due[i]);
            }
        }

        TickType_t after = xTaskGetTickCount();
        TickType_t delay = after - before + pdMS_TO_TICKS(SAMPLE_RATE_MS);

        if(before + TIME_OFFSET > after) {
            vTaskDelay(delay);
        }
    }
}

