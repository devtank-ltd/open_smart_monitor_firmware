#include "hpm.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "pinmap.h"
#include "mqtt-sn.h"
#include "config.h"
#include "logging.h"

// Number of FreeRTOS ticks to wait while trying to receive
#define TICKS_TO_WAIT 100
static int enable = 0;

void hpm_setup() {
    DEBUG_PRINTF("Init HPM");
    const char *value = get_config("HPM");
    enable = value[0] != '0';
}


static void hmp_switch() {
    // Switch UART to HPM
    gpio_set_level(SW_SEL, 0);
    ESP_ERROR_CHECK(uart_set_mode(DEVS_UART, UART_MODE_UART));
    // Flush the input buffer from anything from other device.
    uart_flush(DEVS_UART);
}

typedef union {
    uint16_t d;
    struct {
        uint8_t l;
        uint8_t h;
    };
} unit_entry_t;


static int process_part_measure_response(uint8_t *data) {

    DEBUG_PRINTF("HPM short particle measure msg");
    if (data[1] != 5 || data[2] != 0x04) {
        ERROR_PRINTF("Malformed HMP module particle measure result.");
        return -1;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 7; n++)
        cs += data[n];

    cs = (65536 - cs) % 256;

    uint8_t checksum = data[7];

    if(checksum != cs) {
        ERROR_PRINTF("HPM checksum error; got 0x%02x but expected 0x%02x.", checksum, cs);
        return -1;
    }

    unit_entry_t pm25 = {.h = data[3], .l = data[4]};
    unit_entry_t pm10 = {.h = data[5], .l = data[6]};

    DEBUG_PRINTF("HPM : PM10:%u, PM2.5:%u", (unsigned)pm10.d, (unsigned)pm25.d);

    mqtt_announce_int("PM10",  pm10.d);
    mqtt_announce_int("PM2.5", pm25.d);
    return 8;
}


static int process_part_measure_long_response(uint8_t *data) {

    DEBUG_PRINTF("HPM long particle measure msg");
    if (data[1] != 0x4d || data[2] != 0 || data[3] != 28) { /* 13 2byte data entries + 2 for byte checksum*/
        ERROR_PRINTF("Malformed long HMP module particle measure result.");
        return -1;
    }

    uint16_t cs = 0;

    for(unsigned n = 0; n < 30; n++)
        cs += data[n];

    unit_entry_t checksum = {.h = data[30], .l = data[31]};

    if (checksum.d != cs) {
        ERROR_PRINTF("HPM checksum error; got 0x%02x but expected 0x%02x.", checksum.d, cs);
        return -1;
    }

    unit_entry_t pm25 = {.h = data[6], .l = data[7]};
    unit_entry_t pm10 = {.h = data[8], .l = data[9]};

    DEBUG_PRINTF("HPM : PM10:%u, PM2.5:%u", (unsigned)pm10.d, (unsigned)pm25.d);

    mqtt_announce_int("PM10",  pm10.d);
    mqtt_announce_int("PM2.5", pm25.d);
    return 32;
}


static int process_nack_response(uint8_t *data) {
    if(data[1] == 0x96) {
        ERROR_PRINTF("Negative ACK from HPM module");
        return 2;
    }
    else ERROR_PRINTF("NACK from HPM module broken.");

    return -1;
}


static int process_ack_response(uint8_t *data) {
    DEBUG_PRINTF("HPM ACK");
    if (data[1] == 0xA5) {
        INFO_PRINTF("ACK received from HPM module");
        return 2;
    }
    else ERROR_PRINTF("ACK from HPM module broken.");

    return -1;
}

typedef struct {
    uint8_t id;
    uint8_t len;
    int (*cb)(uint8_t *data);
} hpm_response_t;


static hpm_response_t responses[] = {
    {0x40, 8,  process_part_measure_response},
    {0x42, 32, process_part_measure_long_response},
    {0x96, 2,  process_nack_response},
    {0xA5, 2,  process_ack_response},
    {0,0, 0}
};


int hpm_query() {
    if(!enable) return 0;

    hmp_switch();

    // The body of this function implements my understanding of the protocol laid out by table 4 in
    // the datasheet from Honeywell.

    // Send the request for the measurement
    static uint8_t hpm_send[] = {0x68, 0x01, 0x04, 0x93};
    if(uart_write_bytes(DEVS_UART, (char*)hpm_send, sizeof(hpm_send)) != sizeof(hpm_send)) {
        ERROR_PRINTF("Error querying the HPM module");
        return -1;
    }

    // One of two possibilities:
    // The device responds with a positive acknowledgment, which includes the measurement itself,
    // OR the device responds with a negative ACK which doesn't.
    uint8_t data[128];
    size_t length = 0;

    // Wait for at least eight bytes from the particle sensor.
    for(unsigned i = 0; i < TICKS_TO_WAIT && !length; i++) {
        uart_get_buffered_data_len(DEVS_UART, &length);
        vTaskDelay(1);
    }

    while (length) {
        uint8_t header;
        size_t read_bytes = uart_read_bytes(DEVS_UART, &header, 1, 2000);

        if (read_bytes > 0) {
            if (header) {
                for(hpm_response_t * respond = responses; respond->id; respond++)
                    if (header == respond->id) {
                        data[0] = header;
                        read_bytes = uart_read_bytes(DEVS_UART, data + 1, respond->len - 1, 2000);
                        if ((read_bytes + 1) == respond->len) {
                            int r = respond->cb(data);
                            if (r < 0) {
                                goto unknown_response;
                            }
                            return 0;
                        }
                        else {
                            ERROR_PRINTF("Failed to read message body.");
                            goto unknown_response;
                        }
                    }

                ERROR_PRINTF("Unknown HPM message header 0x%02"PRIx8, header);
                length = (length > sizeof(data))?sizeof(data):length;
                read_bytes = uart_read_bytes(DEVS_UART, data, length-1, 2000);
                if (read_bytes > 0) {
                    for(unsigned n = 0; n < read_bytes; n++) {
                        ERROR_PRINTF("0x%02"PRIx8, data[n]);
                    }
                }
                goto unknown_response;
            } else if (header == 0xFF) {
                /* The odd stray high isn't unreasonable, it can happen. */
                length--;
            }
            else {
                /* The odd stray zero isn't unreasonable, it can happen. */
                length--;
            }
        }
    }
    ERROR_PRINTF("Unable to read HPM message header.");

    return 0;

unknown_response:

    if(length == 0)
        ERROR_PRINTF(" - is it even connected?");
    else
        uart_flush_input(DEVS_UART); /* Bin everything in hope to sync back up. */
    return -1;
}
