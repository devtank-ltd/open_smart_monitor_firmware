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

void measurements_task(void *pvParameters) {
    uint8_t soco_en = get_socoen();
    uint8_t hpm_en = get_hpmen();

    if(hpm_en) hpm_setup();
    adc_setup();
    tsl_setup();
    volume_setup();
    if(soco_en) smart_meter_setup();

    // Take a few samples from various sensors;
    // This will stagger the times when they need to calculate averages etc
    sound_query();
    sound_query();
    sound_query();
    sound_query();

    hdc_query();
    hdc_query();
    hdc_query();

    if(hpm_en) {
        hpm_query();
        hpm_query();
    }
    if(soco_en) {
        smart_meter_query();
        smart_meter_query();
    }

    tsl_query();

    for(;;) {
        // These need to be announced very rarely.
        mqtt_announce_str("sku", "ENV-01");
        mqtt_announce_str("fw", GIT_COMMIT);

        for(int i = 0; i < 10; i++) {
            for(int j = 0; j < 600; j++) {
                printf("sampling.\n");
                // These need to be averaged over ten minutes.
                if(hpm_en) hpm_query();   // smog sensor
                hdc_query();   // humidity and temperature
                sound_query(); // sound
                tsl_query();   // light
                if(soco_en) smart_meter_query();
                //vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            // These need to be announced once every ten minutes or whatever
            query_pulsecount();
            battery_query();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

