#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "driver/i2c.h"

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
        /* UART HW flow control mode */ .flow_ctrl = UART_HW_FLOWCTRL_CTS,
    };
    ESP_ERROR_CHECK(uart_param_config(LORA_UART, &lora));

    esp_err_t err = uart_set_pin(LORA_UART, LORA_UART_TX, LORA_UART_RX, UART_PIN_NO_CHANGE, LORA_UART_CTS);
    if(err != ESP_OK) {
        printf("Trouble %s setting the pins up!\n", esp_err_to_name(err));
        while(1);
    }

    const int uart_buffer_size = 2 * 1024;
    ESP_ERROR_CHECK(uart_driver_install(LORA_UART, uart_buffer_size, uart_buffer_size, 0, NULL, 0));

}

void i2c_setup() {

    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2CPIN_MASTER_SDA;
    conf.scl_io_num = I2CPIN_MASTER_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 10000;

    if (i2c_param_config(I2CBUS, &conf) != ESP_OK)
    	return false;

    if (i2c_driver_install(I2CBUS, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK)
		return false;

    return true;
}

void app_main(void)
{
    notification("ENTRYPOINT REACHED");

    notification("CONFIGURING UARTS AND I2C");
    lora_uart_setup();
//    hpm_uart_setup();
    i2c_setup();
    tsl_init();
    init_smart_meter();
    volume_setup();

    for(;;) {
        
        im_alive();

        // Query the particle sensor
        uint16_t pm25, pm10;
        if(!hpm_query(&pm25, &pm10)) {
            update_pm25(pm25);
            update_pm10(pm10);
        }

        float relative_humidity;
        float temp_celsius;
        hdc_query(&temp_celsius, &relative_humidity);
        update_hum(relative_humidity);
        update_temp(temp_celsius);
        
        uint16_t ch0 = 0;
        uint16_t ch1 = 0;
        tsl_query(&ch0, &ch1);
        update_ch0(ch0);
        update_ch1(ch1);

        query_countis();
        qry_volume();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    fflush(stdout);
    esp_restart();
}
