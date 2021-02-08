#include "stats.h"
#include "config.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "mqtt.h"

#define QUEUESIZE 50
#define STACKSIZE 10000

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
xQueueHandle queues[ARRAY_SIZE(parameter_names)] = {0};

TaskHandle_t xStatsHandle = NULL;
StaticTask_t xStatsBuffer;
StackType_t  xStatsStack[STACKSIZE];

void stats_task(void *pvParameters) {
    int datum;
    for(;;) {
        for(int i = 0; i < ARRAY_SIZE(parameter_names); i++) {

            // Read as many values from the queue as possible
            while(xQueueReceive(queues[i], &datum, 0) == pdTRUE) {
			    stats[i].cumulative += datum;
                if(datum < stats[i].minimum) stats[i].minimum = datum;
                if(datum > stats[i].maximum) stats[i].maximum = datum;
                stats[i].sample_count++;
            }

            /* If enough time has elapsed, then compute the average, and
             * publish the other data
             */
            if(stats[i].updated + stats[i].delta < xTaskGetTickCount()) {
                stats[i].updated = xTaskGetTickCount();
                mqtt_enqueue_int(parameter_names[i], "min", stats[i].minimum);
                mqtt_enqueue_int(parameter_names[i], "max", stats[i].maximum);
                mqtt_enqueue_int(parameter_names[i], "avg", stats[i].cumulative / stats[i].sample_count);
            }

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void stats_init() {
    // Populate the array of queues
    for(int i = 0; i < ARRAY_SIZE(parameter_names); i++) {
        queues[i] = xQueueCreate(QUEUESIZE, sizeof(int));
        stats[i].ready = false;
        stats[i].updated = 0;
        stats[i].sent = 0;
        stats[i].delta = 1 * 60 * 1000 / portTICK_PERIOD_MS;
        stats[i].cumulative = 0;
        stats[i].minimum = 0;
        stats[i].maximum = 0;
        stats[i].sample_count = 0;
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
