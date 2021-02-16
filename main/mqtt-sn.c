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

#define MQTT_SN_MAX_PACKET_LENGTH     (255)
#define MQTT_SN_TYPE_PUBLISH          (0x0C)
#define MQTT_SN_FLAG_QOS_N1           (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN           (0x1 << 4)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)

#define SFNODE '9'

#define BUFFERLEN 255
#define ACKBUFLEN 2550
#define ABS(x)  (x<0)?-x:x

#define DR_REG_RNG_BASE                        0x3ff75144

#define STACKSIZE 9000

static TaskHandle_t xMQTTHandle = NULL;
static StaticTask_t xMQTTBuffer;
static StackType_t  xMQTTStack[STACKSIZE];

static volatile uint16_t dropped = 0;

static void sendthebytes(const char * str, size_t len) {
    while(len)
    {
        esp_err_t err = uart_wait_tx_done(LORA_UART, 200);
        if(err == ESP_OK)
        {
            int sent = uart_tx_chars(LORA_UART, str, len);
            if (sent >= 0)
            {
                len -= sent;
                str += sent;
                if(len)
                    DEBUG_PRINTF("Only printed %d bytes, should have printed %zu. Trying again.", sent, len);
            }
            else if (sent < 0)
            {
                ERROR_PRINTF("Error writing to UART");
            }
        } else {
            ERROR_PRINTF("Trouble %s writing to the LoRa UART.", esp_err_to_name(err));
        }
    }
}

void clear_buffer() {
    //uart_flush(LORA_UART);
}

int await_ack() {
    uint8_t ackbuf[ACKBUFLEN];
    // I have measured that the ACK arrives around 600 ms after the packet finished sending.
    // I've bumped that up to 2000 milliseconds for lots of wiggle-room;
    int received = uart_read_bytes(LORA_UART, ackbuf, ACKBUFLEN, 2000 / portTICK_PERIOD_MS);
    if(received < 0) {
        ERROR_PRINTF("Error getting ACK\n");
        return 0;
    }
    if(received < 25) {
        ERROR_PRINTF("Not enough bytes for ACK");
        return 0;
    }

    // We've just received some number of bytes, which should contain an ACK message
    char ackmsg[ACKBUFLEN];
    const char example[25] = {
       	0x19,
	    0x0c,
	    0x62,
	    0x39,
	    0x61,
	    0x00,
	    0x00,
	    0x5b,
	    0x39,
	    0x38,
	    0x66,
	    0x34,
	    0x61,
	    0x62,
	    0x31,
	    0x34,
	    0x37,
	    0x33,
	    0x39,
	    0x35,
	    0x20,
	    0x61,
	    0x63,
	    0x6b,
	    0x5d
    };
    memcpy(ackmsg, example, example[0]);
    memcpy(ackmsg + 8, mac_addr, 12);

    for(int i = 0; i <= received - 25; i++) {
        if(!memcmp(ackbuf + i, ackmsg, ackmsg[0] - 1)) {
            return 1; // found it
        }
    }
    /*
    ERROR_PRINTF("No ACK received");
    INFO_PRINTF("Here's the ackmsg for comparison");
    for(int i = 0; i < received; i++)
        INFO_PRINTF("\t%2x %2x\t%c %c", ackbuf[i], ackmsg[i], ackbuf[i], ackmsg[i]);
    */

    return 0; // not found
}

// This function was shamelessly stolen from
// https://github.com/njh/DangerMinusOne/blob/master/DangerMinusOne.ino
// and changed to Actual C by me.
static int mqtt_sn_send(const char topic[2], const char * message)
{
    if(!get_mqtten()) return 0;
    char header[7];
    size_t len = strlen(message);

    if (len > (255-7))
        len = 255-7;

    header[0] = sizeof(header) + len + 4;
    header[1] = MQTT_SN_TYPE_PUBLISH;
    header[2] = MQTT_SN_FLAG_QOS_N1 | MQTT_SN_TOPIC_TYPE_SHORT;

    header[3] = topic[0];
    header[4] = topic[1];
    header[5] = 0x00;  // Message ID High
    header[6] = 0x00;  // message ID Low;

    uint16_t crc = modbus_crc((const uint8_t *)message, strlen(message));
    char crcstr[4];
    const char * hex = "0123456789abcdef";

    crcstr[3] = hex[crc      & 0xf];
    crcstr[2] = hex[crc >> 4 & 0xf];
    crcstr[1] = hex[crc >> 8 & 0xf];
    crcstr[0] = hex[crc >>12 & 0xf];

    int i = 0;
    while(1){
        clear_buffer();
        sendthebytes(header, 7);
        sendthebytes(message, len);
        sendthebytes(crcstr, 4);
        if(await_ack()) {
            status_led_set_status(STATUS_LED_OK);
            return 0;
        } else {
            status_led_set_status(STATUS_LED_TROUBLE);
            uint32_t randomNumber = READ_PERI_REG(DR_REG_RNG_BASE) & 0x3ff;
            vTaskDelay(randomNumber);
            if(i > 3) {
                printf("Giving up after %d goes.\n", i + 1);
                dropped++;
                return 1;
            }
        }
        i++;
    }
    return 0;
}

static int mqtt_update(const char ident, const char * msg) {
    char topic[2];
    topic[0] = SFNODE;
    topic[1] = ident;
    printf("%c%c: %s\n", topic[0], topic[1], msg);
    return mqtt_sn_send(topic, msg);
}

void mqtt_announce_dropped() {
    static uint16_t old_dropped = 0;
    if(ABS(dropped - old_dropped) > 5) {
        old_dropped = dropped;
        mqtt_enqueue_int("mqtt_dropped", NULL, dropped);
    }
}

int mqtt_announce_int(const char * key, int val) {
    char msg[BUFFERLEN];
    snprintf(msg, BUFFERLEN - 1, "[%s %s %d];", mac_addr, key, val);
    // workaround for the fact that this snprintf isn't null-terminating the string
    strstr(msg, ";")[0] = '\0';
    return mqtt_update('I', msg);
}

int mqtt_announce_str(const char * key, const char * val) {
    char msg[BUFFERLEN];
    snprintf(msg, BUFFERLEN - 1, "[%s %s %s]", mac_addr, key, val);
    return mqtt_update('I', msg);
}


void mqtt_sn_task(void * pvParameters) {
    char topic[TOPICLEN + SUFFIXLEN + 2];
    for(;;) {
        msg_t msg;
        // 1200000 milliseconds is twenty minutes
        while(xQueueReceive(mqtt_queue, &msg, pdMS_TO_TICKS(1200000) == pdTRUE)) {
            strcpy(topic, msg.topic);
            if(strlen(msg.suffix)) strcat(topic, "-");
            strcat(topic, msg.suffix);

            mqtt_announce_str(topic, msg.payload);
        }
        mqtt_announce_str("sku", "ENV-01");
        mqtt_announce_str("fw", GIT_COMMIT);
        mqtt_announce_dropped();
    }
}

void mqtt_sn_init() {
    xMQTTHandle = xTaskCreateStatic(
                      mqtt_sn_task,
                      "MQTT-SN",
                      STACKSIZE,
                      (void*)1,
                      tskIDLE_PRIORITY + 1,
                      xMQTTStack,
                      &xMQTTBuffer);
}
