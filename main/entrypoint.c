/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"

// The SDK provides the symbols UART_NUM_0, UART_NUM_1 and UART_NUM_2. Better 
// not use those. Use the symbols defined below instead; you know, just in case
// we want to juggle things around a bit which we might want to do.
#define LORAUART  UART_NUM_2
#define HPMUART   UART_NUM_1

unsigned long __stack_chk_guard;
void __stack_chk_guard_setup(void)
{
    __stack_chk_guard = 0xBAAAAAAD;//provide some magic numbers
}

void notification(const char * n) {
    for(int i = 80; i; i--) putchar('*');
    putchar('\n');
    puts(n);
    for(int i = 80; i; i--) putchar('*');
    putchar('\n');
    fflush(stdout);
}


void lora_uart_setup() {
    /* UART setup */
    uart_config_t lora = {
        /* UART baud rate */            .baud_rate = 19200,
        /* UART byte size */            .data_bits = UART_DATA_8_BITS,
        /* UART parity mode */          .parity = UART_PARITY_DISABLE,
        /* UART stop bits */            .stop_bits = UART_STOP_BITS_1,
        /* UART HW flow control mode */ .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//      /* UART HW RTS threshold */     .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(uart_param_config(LORAUART, &lora));

    //esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
    if(uart_set_pin(LORAUART, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_FAIL)
        printf("Error in uart_set_pin!\n");
    notification("UART PINS CONFIGURED");

    const int uart_buffer_size = (1024 * 2);
    ESP_ERROR_CHECK(uart_driver_install(LORAUART, uart_buffer_size, uart_buffer_size, 10, NULL, 0));

    notification("PRINTING TO LORA");
    if(uart_tx_chars(LORAUART, "Hello, world! \n", 15) != 15)
        printf("Error printing a string to the UART");
    ESP_ERROR_CHECK(uart_wait_tx_done(LORAUART, 100));
}

void hpm_uart_setup() {
    /* UART setup */
    uart_config_t hpm = {
        /* UART baud rate */            .baud_rate = 9600,
        /* UART byte size */            .data_bits = UART_DATA_8_BITS,
        /* UART parity mode */          .parity = UART_PARITY_DISABLE,
        /* UART stop bits */            .stop_bits = UART_STOP_BITS_1,
        /* UART HW flow control mode */ .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        /* UART HW RTS threshold */     .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(uart_param_config(HPMUART, &hpm));

    //esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
    if(uart_set_pin(HPMUART, 9, 10, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_FAIL)
        printf("Error in uart_set_pin!\n");
    notification("UART PINS CONFIGURED");

    const int uart_buffer_size = (1024 * 2);
    ESP_ERROR_CHECK(uart_driver_install(HPMUART, uart_buffer_size, uart_buffer_size, 10, NULL, 0));
}

int hpm_query() {
    static char hpm_send[4] = {0x68, 0x01, 0x04, 0x93};
    if(uart_tx_chars(HPMUART, hpm_send, 4) != 4) {
        printf("Error querying the HPM module\n");
    }
    return 0;
}

void app_main(void)
{
    notification("ENTRYPOINT REACHED");

    notification("CONFIGURING UART2 for LORA");
    lora_uart_setup();

    notification("CONFIGURING UART1 for HPM MODULE");
    //hpm_uart_setup();

    notification("QUERYING PARTICULATE MATTER SENSOR");
    //printf("dust: %d", hpm_query());

    /* Print chip information */
    for (int i = 9; i >= 0; i--) {
        printf("Echoing %d more lines ... ", i);
        
        uint8_t data[128];
        data[0] = 0;
        int length = 0;
        uart_get_buffered_data_len(LORAUART, (size_t*)&length);
        length = uart_read_bytes(LORAUART, data, length, 2000);
        
        for(int i = 0; i < length; i++) putchar(data[i]);
        putchar('\n');
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");

    fflush(stdout);
    esp_restart();
}
