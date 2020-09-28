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
                //vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            printf("announcing.\n");

            // Also announce the averages, minima, etc.
            if(hpm_en) hpm_announce();
            hdc_announce();
            sound_announce();

            // These need to be announced once every ten minutes or whatever
            tsl_announce();   // light
            if(soco_en) smart_meter_query();
            water_volume_query();
            light_volume_query();
            battery_query();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

