#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "logging.h"
#include "pinmap.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "mac.h"

#define MQTT_SN_MAX_PACKET_LENGTH     (255)
#define MQTT_SN_TYPE_PUBLISH          (0x0C)
#define MQTT_SN_FLAG_QOS_N1           (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN           (0x1 << 4)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)

#define SFNODE '9'

#define BUFLEN 255


static void sendthebytes(const char * str, size_t len) {
    while(len)
    {
        esp_err_t err = uart_wait_tx_done(LORA_UART, 200);
        if(err == ESP_OK)
        {
            int sent = uart_tx_chars(LORA_UART, str, len);
            if (sent > 0)
            {
                len -= sent;
                str += sent;
                if(len)
                    DEBUG_PRINTF("Only printed %d bytes, should have printed %zu. Trying again.\n", sent, len);
            }
            else if (sent < 0)
            {
                ERROR_PRINTF("Error writing to UART\n");
            }
        } else {
            ERROR_PRINTF("Trouble %s writing to the LoRa UART.\n", esp_err_to_name(err));
        }
    }
}


// This function was shamelessly stolen from
// https://github.com/njh/DangerMinusOne/blob/master/DangerMinusOne.ino
// and changed to Actual C by me.
static void mqtt_sn_send(const char topic[2], const char * message, bool retain)
{
    char header[7];
    size_t len = strlen(message);

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
    if(retain)
       header[2] |= MQTT_SN_FLAG_RETAIN;

    header[3] = topic[0];
    header[4] = topic[1];
    header[5] = 0x00;  // Message ID High
    header[6] = 0x00;  // message ID Low;

    sendthebytes(header, 7);
    sendthebytes(message, len);

}

static void mqtt_update(const char ident, const char * msg) {
    char topic[2];
    topic[0] = SFNODE;
    topic[1] = ident;
    printf("%c%c: %s\n", topic[0], topic[1], msg);
    mqtt_sn_send(topic, msg, 1);
}

void heartbeat() {
    mqtt_update('f', "I'm alive");
}

void mqtt_announce_int(char * key, int val) {
    char msg[BUFLEN];
    snprintf(msg, BUFLEN - 1, "%s %s %d ", mac_addr, key, val);
    mqtt_update('I', msg);
}
