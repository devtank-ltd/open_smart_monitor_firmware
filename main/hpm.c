#include "devices.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "pinmap.h"

void hpm_uart_setup() {
    /* UART setup */
    uart_config_t hpm = {
        /* UART baud rate */            .baud_rate = 9600,
        /* UART byte size */            .data_bits = UART_DATA_8_BITS,
        /* UART parity mode */          .parity = UART_PARITY_DISABLE,
        /* UART stop bits */            .stop_bits = UART_STOP_BITS_1,
        /* UART HW flow control mode */ .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(HPMUART, &hpm));

    //esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
    if(uart_set_pin(HPMUART, HPMUART_TX, HPMUART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_FAIL)
        printf("Error in uart_set_pin!\n");

    const int uart_buffer_size = (1024 * 2);
    ESP_ERROR_CHECK(uart_driver_install(HPMUART, uart_buffer_size, uart_buffer_size, 10, NULL, 0));
}

int hpm_query(int * pm25, int * pm10) {
    // The body of this function implements my understanding of the protocol laid out by table 4 in
    // the datasheet from Honeywell.

    // Send the request for the measurement
    static char hpm_send[4] = {0x68, 0x01, 0x04, 0x93};
    if(uart_tx_chars(HPMUART, hpm_send, 4) != 4) {
        printf("Error querying the HPM module\n");
        return -1;
    }

    // One of two possibilities:
    // The device responds with a positive acknowledgment, which includes the measurement itself, 
    // OR the device responds with a negative ACK which doesn't.
    uint8_t data[128];
    int length;
    uart_get_buffered_data_len(LORAUART, (size_t*)&length);
    length = uart_read_bytes(LORAUART, data, length, 2000);

    // Check if we got a negative ACK and bail out if so.
    // The negative ACK is two bytes long and reads: 0x96 0x96
    if(length == 2) {
       if(data[0] == 0x96 && data[1] == 0x96) {
            printf("Negative ACK from HPM module");
            return -1;
        } else {
            goto unknown_response;
        }
    }

    if(length == 8) {
        uint8_t head = data[0];
        uint8_t len = data[1];
        uint8_t cmd = data[2];
        uint8_t df1 = data[3];
        uint8_t df2 = data[4];
        uint8_t df3 = data[5];
        uint8_t df4 = data[6];
        uint8_t checksum = data[7];

        if(checksum != (65536 - (head + len + cmd + df1 + df2 + df3 + df4) % 256)) {
            printf("HPM checksum error ");
            goto unknown_response;
        }

        * pm25 = df1 * 256 + df2;
        * pm10 = df3 * 256 + df4;
        return 0;
    }

unknown_response:
    printf("Unknown response from HPM module ");
    for(int i = 0; i < length; i++) printf("0x%1x ", data[i]);
    if(length == 0) printf(" - is it even connected?");
    printf("\n");
    return -1;
}
