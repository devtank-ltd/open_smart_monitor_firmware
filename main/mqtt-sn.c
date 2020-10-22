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
#include "socomec.h"
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

mqtt_stats_t mqtt_humidity_stats = {0};
mqtt_stats_t mqtt_sound_stats = {0};
mqtt_stats_t mqtt_temperature_stats = {0};
mqtt_datum_t mqtt_battery_pc_datum = {0};
mqtt_datum_t mqtt_battery_mv_datum = {0};
mqtt_datum_t mqtt_import_energy_datum = {0};
mqtt_datum_t mqtt_export_energy_datum = {0};
mqtt_datum_t mqtt_water_meter_datum = {0};
mqtt_datum_t mqtt_gas_meter_datum = {0};
mqtt_stats_t mqtt_current1_stats = {0};
mqtt_stats_t mqtt_current2_stats = {0};
mqtt_stats_t mqtt_current3_stats = {0};
mqtt_stats_t mqtt_voltage1_stats = {0};
mqtt_stats_t mqtt_voltage2_stats = {0};
mqtt_stats_t mqtt_voltage3_stats = {0};
mqtt_stats_t mqtt_pf_stats = {0};
mqtt_stats_t mqtt_pf_sign_stats = {0};
mqtt_stats_t mqtt_pm10_stats = {0};
mqtt_stats_t mqtt_pm25_stats = {0};
mqtt_stats_t mqtt_visible_light_stats = {0};

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
    ERROR_PRINTF("No ACK received\n");
    INFO_PRINTF("Here's the ackmsg for comparison");
    for(int i = 0; i < received; i++)
        INFO_PRINTF("\t%2x %2x\t%c %c", ackbuf[i], ackmsg[i], ackbuf[i], ackmsg[i]);

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
            return 0;
        } else {
            printf("ACK not received!\n");
            uint32_t randomNumber = READ_PERI_REG(DR_REG_RNG_BASE) & 0x3ff;
            printf("Sleeping for %u ticks.", randomNumber);
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

int heartbeat() {
    return mqtt_update('f', "I'm alive");
}

int mqtt_announce_dropped() {
    static uint16_t old_dropped = 0;
    return mqtt_delta_announce_int("mqtt_dropped", (uint16_t*)&dropped, &old_dropped, 1);
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

int mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta) {
    if(ABS(*val - *old) > delta) {
        *old = *val;
        return mqtt_announce_int(key, * val);
    }
    return 0;
}

void mqtt_announce_datum(const char * key, mqtt_datum_t * datum) {
    if(!datum->ready || (xTaskGetTickCount() < datum->updated + datum->delta))
        return; // The datum is not ready to be sent over.
    if(!mqtt_announce_int(key, datum->value))
        datum->sent = xTaskGetTickCount();
}

void mqtt_announce_stats(const char * key, mqtt_stats_t * stats) {
    char dkey[100];
    bool ready = true;
    if(!stats->ready || (xTaskGetTickCount() < stats->updated + stats->delta))
        return; // The datum is not ready to be sent over.

    strcpy(dkey, key);
    strcat(dkey, "-min");
    ready &= !mqtt_announce_int(dkey, stats->minimum);

    strcpy(dkey, key);
    strcat(dkey, "-max");
    ready &= mqtt_announce_int(dkey, stats->maximum);

    strcpy(dkey, key);
    strcat(dkey, "-avg");
    ready &= mqtt_announce_int(dkey, stats->average);

    if(ready)
        stats->sent = xTaskGetTickCount();
}

void mqtt_datum_update(mqtt_datum_t * datum, int32_t value) {
    datum->value = value;
    datum->updated = xTaskGetTickCount();
    datum->ready = true;
    if(datum->sent > datum->updated)
        datum->sent = 0; // in case of roll-over.
}

void mqtt_stats_update_delta(mqtt_stats_t * stats, int32_t mins) {
    stats->delta = mins * 60 * 1000 * portTICK_PERIOD_MS;
}
void mqtt_datum_update_delta(mqtt_datum_t * datum, int32_t mins) {
    datum->delta = mins * 60 * 1000 * portTICK_PERIOD_MS;
}

void mqtt_task(void * pvParameters) {
    for(;;) {
        mqtt_announce_str("sku", "ENV-01");
        mqtt_announce_str("fw", GIT_COMMIT);
        for(int i = 0; i < 60; i++) {
            TickType_t before = xTaskGetTickCount();
            mqtt_announce_datum("WaterMeter", &mqtt_water_meter_datum);
            mqtt_announce_stats("sound", &mqtt_sound_stats);
            mqtt_announce_stats("pm25", &mqtt_pm25_stats);
            mqtt_announce_stats("pm10", &mqtt_pm10_stats);
            mqtt_announce_stats("temperature", &mqtt_temperature_stats);
            mqtt_announce_stats("visible-light", &mqtt_visible_light_stats);
            mqtt_announce_stats("pm25", &mqtt_pm25_stats);
            mqtt_announce_stats("pm10", &mqtt_pm10_stats);
            mqtt_announce_stats("temperature", &mqtt_temperature_stats);
            mqtt_announce_stats("humidity", &mqtt_humidity_stats);
            mqtt_announce_stats("sound", &mqtt_sound_stats);
            mqtt_announce_datum("battery-percent", &mqtt_battery_pc_datum);
            mqtt_announce_datum("ImportEnergy", &mqtt_import_energy_datum);
            mqtt_announce_datum("ExportEnergy", &mqtt_export_energy_datum);
            mqtt_announce_datum("WaterMeter", &mqtt_water_meter_datum);
            mqtt_announce_stats("PowerFactor", &mqtt_pf_stats);
            mqtt_announce_stats("PFLeadLag", &mqtt_pf_sign_stats);
            mqtt_announce_stats("Current1", &mqtt_current1_stats);
            mqtt_announce_stats("Current2", &mqtt_current2_stats);
            mqtt_announce_stats("Current3", &mqtt_current3_stats);
            mqtt_announce_stats("Voltage1", &mqtt_voltage1_stats);
            mqtt_announce_stats("Voltage2", &mqtt_voltage2_stats);
            mqtt_announce_stats("Voltage3", &mqtt_voltage3_stats);
            mqtt_announce_datum("battery-millivolts", &mqtt_battery_mv_datum);
            TickType_t after = xTaskGetTickCount();
            TickType_t window = 15 * 60 * 1000; // Fifteen minutes
            TickType_t delay = (before + window) - after;

            INFO_PRINTF("Waiting %f minutes before doing another round of announcements\n", delay / 60.0 / portTICK_PERIOD_MS);
            vTaskDelay(delay);
        }
    }
}
