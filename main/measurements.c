#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"
#include "logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void measurements_task(void *pvParameters) {
    for(;;) {
        // These need to be announced very rarely.
        mqtt_announce_str("sku", "ENV-01");
        mqtt_announce_str("fw", GIT_COMMIT);

        for(int i = 0; i < 2; i++) 
            for(int j = 0; j < 6000; j++) {
                // These need to be averaged over ten minutes.
                hpm_query();   // smog sensor
                hdc_query();   // humidity and temperature
                tsl_query();   // light
                sound_query(); // sound
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            // These need to be announced once every ten minutes or whatever
            smart_meter_query();
            water_volume_query();
            light_volume_query();
            battery_query();

        }
    }
}

