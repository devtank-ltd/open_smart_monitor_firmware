#include "stats.h"
#include "config.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "mqtt.h"
#include "status_led.h"
#include "logging.h"

#define QUEUESIZE 50
#define STACKSIZE 10000

// FIXME: change this to use one queue instead of a fazillion.

typedef volatile struct __ {
    bool ready;
    TickType_t updated;
    TickType_t sent;
    TickType_t delta;
    int64_t cumulative;
    int32_t minimum;
    int32_t maximum;
    unsigned int sample_count;
} stats_t;


int last_updated[ARRAY_SIZE(parameter_names)] = {0};
stats_t stats[ARRAY_SIZE(parameter_names)];
xQueueHandle stats_queue;

TaskHandle_t xStatsHandle = NULL;
StaticTask_t xStatsBuffer;
StackType_t  xStatsStack[STACKSIZE];

void stats_task(void *pvParameters) {
    sample_t sample;
    for(;;) {

        // Read as many values from the queue as possible
        while(xQueueReceive(stats_queue, &sample, 0) == pdTRUE) {
            // FIXME: the maximum and minimum need to be assigned if the sample count is zero
            stats[sample.parameter].cumulative += sample.sample;
            if(!stats[sample.parameter].sample_count) {
                stats[sample.parameter].minimum = sample.sample;
                stats[sample.parameter].maximum = sample.sample;
            } else {
                if(sample.sample < stats[sample.parameter].minimum) 
                    stats[sample.parameter].minimum = sample.sample;
                if(sample.sample > stats[sample.parameter].maximum)
                    stats[sample.parameter].maximum = sample.sample;
            }
            stats[sample.parameter].sample_count++;
        }

        for(int i = 0; i < ARRAY_SIZE(parameter_names); i++) {

            /* If enough time has elapsed, then compute the average, and
             * publish the other data
             */
            if(stats[i].updated + stats[i].delta < xTaskGetTickCount()) {
                stats[i].updated = xTaskGetTickCount();
                if(stats[i].sample_count) {
                    mqtt_enqueue_int(parameter_names[i], "min", stats[i].minimum);
                    mqtt_enqueue_int(parameter_names[i], "max", stats[i].maximum);
                    mqtt_enqueue_int(parameter_names[i], "avg", stats[i].cumulative / stats[i].sample_count);
                    mqtt_enqueue_int(parameter_names[i], "cnt", stats[i].sample_count);
                    stats[i].sample_count = 0;
                    stats[i].cumulative = 0;
                }
            }

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void stats_enqueue_sample(int parameter, int value) {
    sample_t sample;
    sample.parameter = parameter;
    sample.sample = value;
    if(xQueueSend(stats_queue, &sample, pdMS_TO_TICKS(100)) != pdPASS) {
        DEBUG_PRINTF("Could not enqueue a sample for %s", parameter_names[parameter]);
        status_led_set_status(STATUS_LED_TROUBLE);
    } else {
        DEBUG_PRINTF("Enqueued %d for %s", value, parameter_names[parameter]);
    }
}


void stats_init() {
    stats_queue = xQueueCreate(QUEUESIZE, sizeof(msg_t));
    // Populate the array of queues
    for(int i = 0; i < ARRAY_SIZE(parameter_names); i++) {
        stats[i].ready = false;
        stats[i].updated = 0;
        stats[i].sent = 0;
        stats[i].delta = 1 * 60 * 1000 / portTICK_PERIOD_MS;
        stats[i].cumulative = 0;
        stats[i].minimum = 0;
        stats[i].maximum = 0;
        stats[i].sample_count = 0;
        stats[i].delta = get_timedelta(parameter_names[i]);
    }

    xStatsHandle = xTaskCreateStatic(
                      stats_task, /* Function that implements the task. */
                      "STATSTASK",     /* Text name for the task. */
                      STACKSIZE,       /* Number of indexes in the xStack array. */
                      (void*)1,        /* Parameter passed into the task. */
                      tskIDLE_PRIORITY,/* Priority at which the task is created. */
                      xStatsStack,     /* Array to use as the task's stack. */
                      &xStatsBuffer);  /* Variable to hold the task's data structure. */

}
