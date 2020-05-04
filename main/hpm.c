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


int hpm_query() {
    if(!enable) return 0;

    hmp_switch();

    uint16_t pm25, pm10;
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
    for(unsigned i = 0; i < TICKS_TO_WAIT && length < 8; i++) {
        uart_get_buffered_data_len(DEVS_UART, &length);
        vTaskDelay(1);
    }
    if(length < 8)
        goto unknown_response;

    uart_get_buffered_data_len(DEVS_UART, &length);
    length = (length > sizeof(data))?sizeof(data):length;
    length = uart_read_bytes(DEVS_UART, data, length, 2000);

    // Check if we got a negative ACK and bail out if so.
    // The negative ACK is two bytes long and reads: 0x96 0x96
    if(length == 2) {
       if(data[0] == 0x96 && data[1] == 0x96) {
            ERROR_PRINTF("Negative ACK from HPM module");
            return -1;
        } else {
            goto unknown_response;
        }
    }

    if(length != 8)
        goto unknown_response;

    uint8_t head = data[0];
    uint8_t len = data[1];
    uint8_t cmd = data[2];
    uint8_t df1 = data[3];
    uint8_t df2 = data[4];
    uint8_t df3 = data[5];
    uint8_t df4 = data[6];
    uint8_t checksum = data[7];

    uint8_t cs = (65536 - (head + len + cmd + df1 + df2 + df3 + df4) % 256);

    if(checksum != cs) {
        ERROR_PRINTF("HPM checksum error; got 0x%02x but expected 0x%02x.", checksum, cs);
        goto unknown_response;
    }

    pm25 = df1 * 256 + df2;
    pm10 = df3 * 256 + df4;

    DEBUG_PRINTF("HPM : PM10:%u, PM2.5:%u", (unsigned)pm10, (unsigned)pm25);

    mqtt_announce_int("PM10",  pm10);
    mqtt_announce_int("PM2.5", pm25);
    return 0;

unknown_response:
    if(length > 128)
        length = 128;
    ERROR_PRINTF("Unknown response from HPM module, length %zu, as follows:", length);
    for(int i = 0; i < length; i++) {
        ERROR_PRINTF("0x%02x ", data[i]);
    }
    if(length == 0) {
        ERROR_PRINTF(" - is it even connected?");
    }
    return -1;
}
