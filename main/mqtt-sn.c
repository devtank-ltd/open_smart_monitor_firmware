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
#define ABS(x)  (x<0)?-x:x

#define DR_REG_RNG_BASE                        0x3ff75144

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
    uart_flush(LORA_UART);
}

int await_ack() {
    uint8_t ackbuf[BUFLEN];
    uart_read_bytes(LORA_UART, ackbuf, BUFLEN, 200);

    // We've just received some number of bytes, which should contain an ACK message
    char ackmsg[BUFLEN];
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
    memcpy(&ackmsg, example, example[0]);
    memcpy(ackmsg + 8, mac_addr, 12);

    for(int i = 0; i < 64; i++) {
        if(!memcmp(ackbuf + i, ackmsg, ackmsg[0] - 1)) return 1; // found it
    }
    printf("Couldn't find the ACK message in the buffer. Here it is:\n");
    for(int i = 0; i < BUFLEN; i++)
        printf("\t%x\t%c\n", ackbuf[i], ackbuf[i]);
    printf("And here's the ackmsg for comparison");
    for(int i = 0; i < ackmsg[0]; i++)
        printf("\t%0x %0x\t%c %c\n", ackbuf[i], ackmsg[i], ackbuf[i], ackmsg[i]);

    return 0; // not found
}

// This function was shamelessly stolen from
// https://github.com/njh/DangerMinusOne/blob/master/DangerMinusOne.ino
// and changed to Actual C by me.
static void mqtt_sn_send(const char topic[2], const char * message)
{
    char header[7];
    size_t len = strlen(message);

    if (len > (255-7))
        len = 255-7;

    header[0] = sizeof(header) + len;
    header[1] = MQTT_SN_TYPE_PUBLISH;
    header[2] = MQTT_SN_FLAG_QOS_N1 | MQTT_SN_TOPIC_TYPE_SHORT;

    header[3] = topic[0];
    header[4] = topic[1];
    header[5] = 0x00;  // Message ID High
    header[6] = 0x00;  // message ID Low;

    int i = 0;
    while(1){
        clear_buffer();
        sendthebytes(header, 7);
        sendthebytes(message, len);
        if(await_ack()) {
            return;
        } else {
            printf("ACK not received!\n");
            uint32_t randomNumber = READ_PERI_REG(DR_REG_RNG_BASE) & 0x3ff;
            printf("Sleeping for %d ticks.", randomNumber);
            vTaskDelay(randomNumber);
            if(i > 3) {
                printf("Giving up after %d goes.\n", i + 1);
                dropped++;
                return;
            }
        }
        i++;
    }
}

static void mqtt_update(const char ident, const char * msg) {
    char topic[2];
    topic[0] = SFNODE;
    topic[1] = ident;
    printf("%c%c: %s\n", topic[0], topic[1], msg);
    mqtt_sn_send(topic, msg);
}

void heartbeat() {
    mqtt_update('f', "I'm alive");
}

void mqtt_announce_dropped() {
    static uint16_t old_dropped = 0;
    mqtt_delta_announce_int("mqtt_dropped", dropped, old_dropped, 1);
}

void mqtt_announce_int(char * key, int val) {
    char msg[BUFLEN];
    snprintf(msg, BUFLEN - 1, "[%s %s %d];", mac_addr, key, val);
    // workaround for the fact that this snprintf isn't null-terminating the string
    strstr(msg, ";")[0] = '\0';
    mqtt_update('I', msg);
}

void mqtt_announce_str(char * key, char * val) {
    char msg[BUFLEN];
    snprintf(msg, BUFLEN - 1, "[%s %s %s];", mac_addr, key, val);
    mqtt_update('I', msg);
}

void mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta) {
    if(ABS(*val - *old) > delta) {
        *old = *val;
        mqtt_announce_int(key, * val);
    }
}
