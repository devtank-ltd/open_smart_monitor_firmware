#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pinmap.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"

#define MQTT_SN_MAX_PACKET_LENGTH     (255)
#define MQTT_SN_TYPE_PUBLISH          (0x0C)
#define MQTT_SN_FLAG_QOS_N1           (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN           (0x1 << 4)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)

#define SFNODE      '9'

// This funcion was shamelessly stolen from 
// https://github.com/njh/DangerMinusOne/blob/master/DangerMinusOne.ino
// and changed to Actual C by me.
void mqtt_sn_send(const char topic[2], const char * message, bool retain)
{
    char header[7];
    unsigned len = strlen(message);

    if (len > (255-7))
        len = 255-7;

    header[0] = sizeof(header) + len;
    header[1] = MQTT_SN_TYPE_PUBLISH;
    header[2] = MQTT_SN_FLAG_QOS_N1 | MQTT_SN_TOPIC_TYPE_SHORT;

    /* Setting the RETAIN flag means that the broker will store the message
     * under the topic, so that if a subscriber connects after the message
     * has been published, the broker will immediately send the message to the
     * subscriber.
     */
    if (retain) header[2] |= MQTT_SN_FLAG_RETAIN;

    header[3] = topic[0];
    header[4] = topic[1];
    header[5] = 0x00;  // Message ID High
    header[6] = 0x00;  // message ID Low;

    if(uart_wait_tx_done(LORA_UART, 100) == ESP_ERR_TIMEOUT) printf("Timeout while waiting1 for the MQTT packet to leave\n");
    uart_tx_chars(LORA_UART, header, 7);
    if(uart_wait_tx_done(LORA_UART, 100) == ESP_ERR_TIMEOUT) printf("Timeout while waiting2 for the MQTT packet to leave\n");
    uart_tx_chars(LORA_UART, message, len);
    if(uart_wait_tx_done(LORA_UART, 100) == ESP_ERR_TIMEOUT) printf("Timeout while waiting3 for the MQTT packet to leave\n");

    printf("sent msg %s\n", message);
}

void mqtt_update(const char ident, const char * msg) {
    char topic[2];
    topic[0] = SFNODE;
    topic[1] = ident;
    mqtt_sn_send(topic, msg, 1);
}
