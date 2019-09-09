#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"

#include "pinmap.h"
#include "devices.h"
#include "mqtt-sn.h"

unsigned long __stack_chk_guard;
void __stack_chk_guard_setup(void)
{
    __stack_chk_guard = 0xBAAAAAAD;//provide some magic numbers
}

void notification(const char * n) {
    for(int i = 30; i; i--) putchar('*');
    putchar(' ');
    puts(n);
    putchar('\r');
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
    };
    ESP_ERROR_CHECK(uart_param_config(LORA_UART, &lora));

    //esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
    if(uart_set_pin(LORA_UART, LORA_UART_TX, LORA_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_FAIL)
        printf("Error in uart_set_pin!\n");
    notification("UART PINS CONFIGURED");

    const int uart_buffer_size = (1024 * 2);
    ESP_ERROR_CHECK(uart_driver_install(LORA_UART, uart_buffer_size, uart_buffer_size, 10, NULL, 0));

    notification("PRINTING TO LORA");
//    if(uart_tx_chars(LORA_UART, "Hello, world! \n", 15) != 15)
//        printf("Error printing a string to the UART");
//    ESP_ERROR_CHECK(uart_wait_tx_done(LORA_UART, 100));
    mqtt_sn_send("f5", "fish5 is a live", 0);
}

void app_main(void)
{
    notification("ENTRYPOINT REACHED");

    notification("CONFIGURING UART2 for LORA");
    lora_uart_setup();

    notification("CONFIGURING UART1 for HPM MODULE");
    hpm_uart_setup();

    notification("QUERYING PARTICULATE MATTER SENSOR");
    uint16_t pm25, pm10;

    if(!hpm_query(&pm25, &pm10)) {
        printf("PM2.5 = %d, PM10 = %d\n", pm25, pm10);
    }

    notification("QUERYING TEMP & HUMIDITY SENSOR");
    float relative_humidity;
    float temp_celsius;
    hdc_query(&temp_celsius, &relative_humidity);
    printf("Temperature: %f degrees celcius\n", temp_celsius);
    printf("%f%% relative humidity\n", relative_humidity);

    notification("AMBIENT LIGHT SENSOR");
    uint16_t ch0 = 0;
    uint16_t ch1 = 0;
    tsl_query(&ch0, &ch1);
    printf("CH0 (visible light) = %d, CH1 (infrared) = %d\n", ch0, ch1);


    /* Print chip information */
    for (int i = 9; i >= 0; i--) {
        printf("Echoing %d more lines ... ", i);
        
        uint8_t data[128];
        data[0] = 0;
        int length = 0;
        uart_get_buffered_data_len(LORA_UART, (size_t*)&length);
        length = uart_read_bytes(LORA_UART, data, length, 2000);
        
        for(int i = 0; i < length; i++) putchar(data[i]);
        putchar('\n');
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");

    fflush(stdout);
    esp_restart();
}
