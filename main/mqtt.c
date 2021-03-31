#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "logging.h"
#include "mac.h"
#include "mqtt.h"
#include "string.h"
#include "status_led.h"
#include "stats.h"

#define QDELAY 2000
#define QUEUESIZE 100

volatile int uptime = 0;

xQueueHandle mqtt_queue = 0;

void mqtt_enqueue_int(const char * parameter, const char * suffix, int val) {
    msg_t msg;

    msg.timestamp = mqtt_get_uptime();

    strncpy(msg.topic, parameter, TOPICLEN);
    if(suffix) strncpy(msg.suffix, suffix,  SUFFIXLEN);
    else strncpy(msg.suffix, "", SUFFIXLEN);
    strncpy(msg.mac, mac_addr, MACLEN);

    /* This implementation of snprintf is not guaranteed to null-terminate the
     * string, hence the stupid strstr hack.
     */
    snprintf(msg.payload, PAYLOADLEN, "%d.%03d,", val / PRECISION, val % PRECISION);
    strstr(msg.payload, ",")[0] = '\0';

    if(xQueueSend(mqtt_queue, &msg, QDELAY) != pdPASS) {
        status_led_set_status(STATUS_LED_TROUBLE);
        if(suffix) 
            ERROR_PRINTF("Error enqueueing MQTT %s-%s %d\n", parameter, suffix, val);
        else
            ERROR_PRINTF("Error enqueueing MQTT %s %d\n", parameter, val);
    }

}

static void uptime_increment(void * arg) {
    uptime++;
}

int mqtt_get_uptime() {
    return uptime;
}

void mqtt_init() {
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &uptime_increment,
        .name = "uptime_increment",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // fire the timer every second

    mqtt_queue = xQueueCreate(QUEUESIZE, sizeof(msg_t));
}
