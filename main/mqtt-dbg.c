#include <stdint.h>
#include <string.h>
#include "logging.h"
#include "pinmap.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "mqtt-sn.h"
#include "commit.h"
#include "mac.h"
#include "mqtt.h"
#include "socomec.h"
#include "status_led.h"
#include "config.h"
#include "freertos/task.h"

#define STACKSIZE 3000

static TaskHandle_t xMQTTHandle = NULL;
static StaticTask_t xMQTTBuffer;
static StackType_t  xMQTTStack[STACKSIZE];

static volatile uint16_t dropped = 0;

void mqtt_dbg_task(void * pvParameters) {
    for(;;) {
        msg_t msg;
        // 1200000 milliseconds is twenty minutes
        while(xQueueReceive(mqtt_queue, &msg, pdMS_TO_TICKS(100) == pdTRUE)) {
            DEBUG_PRINTF("%s %d %s-%s %s", msg.mac, msg.timestamp, msg.topic, msg.suffix, msg.payload);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void mqtt_dbg_init() {
    xMQTTHandle = xTaskCreateStatic(
                      mqtt_dbg_task,
                      "MQTT-DBG",
                      STACKSIZE,
                      (void*)1,
                      tskIDLE_PRIORITY + 1,
                      xMQTTStack,
                      &xMQTTBuffer);
}
