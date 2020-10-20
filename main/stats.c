#include "stats.h"
#include "mqtt-sn.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void stats(int32_t * arr, unsigned int len, mqtt_stats_t * stats) {
    stats->minimum = arr[0];
    stats->maximum = arr[0];
    stats->average = 0;

    for(int i = 0; i < len; i++) {
        uint16_t samp = arr[i];
        if(stats->minimum > samp) stats->minimum = samp;
        if(stats->maximum < samp) stats->maximum = samp;
        stats->average += samp;
    }
    stats->average /= len;
    stats->updated = xTaskGetTickCount();
}
