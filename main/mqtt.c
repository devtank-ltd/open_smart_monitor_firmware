#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mac.h"
#include "mqtt.h"

#define QUEUESIZE 100
#define SUFFIXLEN 4
#define TOPICLEN 20
#define PAYLOADLEN 20

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

    if(xQueueSend(mqtt_queue, msg, QDELAY) != pdPASS)
        ERROR_PRINTF("Error enqueueing MQTT %s-%s %d\n", parameter, suffix, val);

}

void mqtt_init() {
    xQueueCreate(QUEUESIZE, sizeof(int));
}
