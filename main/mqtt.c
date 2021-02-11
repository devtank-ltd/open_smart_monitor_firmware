#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "logging.h"
#include "mac.h"
#include "mqtt.h"
#include "string.h"
#include "status_led.h"

#define QDELAY 2000
#define QUEUESIZE 100

xQueueHandle mqtt_queue = 0;

void mqtt_enqueue_int(const char * parameter, const char * suffix, int val) {
    msg_t msg;

    msg.timestamp = esp_timer_get_time();

    strncpy(msg.topic, parameter, TOPICLEN);
    if(suffix) strncpy(msg.suffix, suffix,  SUFFIXLEN);
    else strncpy(msg.suffix, "", SUFFIXLEN);
    strncpy(msg.mac, mac_addr, MACLEN);

    /* This implementation of snprintf is not guaranteed to null-terminate the
     * string, hence the stupid strstr hack.
     */
    snprintf(msg.payload, PAYLOADLEN, "%d,", val);
    strstr(msg.payload, ",")[0] = '\0';

    if(xQueueSend(mqtt_queue, &msg, QDELAY) != pdPASS) {
        status_led_set_status(STATUS_LED_TROUBLE);
        ERROR_PRINTF("Error enqueueing MQTT %s-%s %d\n", parameter, suffix, val);
    }

}

void mqtt_init() {
    mqtt_queue = xQueueCreate(QUEUESIZE, sizeof(int));
}
