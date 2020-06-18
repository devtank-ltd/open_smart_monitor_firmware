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

#include "hdc.h"
#include "adc.h"
#include "hpm.h"
#include "tsl.h"
#include "volume.h"
#include "socomec.h"
#include "mqtt-sn.h"
#include "mac.h"
#include "config.h"
#include "status_led.h"
#include "logging.h"
#include "commit.h"
#include "math.h"

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
    DEBUG_PRINTF("Init lora uart.");
    ESP_ERROR_CHECK(uart_param_config(LORA_UART, &lora));

    esp_err_t err = uart_set_pin(LORA_UART, LORA_UART_TX, LORA_UART_RX, UART_PIN_NO_CHANGE, LORA_UART_CTS);
    if(err != ESP_OK) {
        ERROR_PRINTF("Trouble %s setting the pins up!", esp_err_to_name(err));
        while(1);
    }

    const int uart_buffer_size = 2 * 1024;
    ESP_ERROR_CHECK(uart_driver_install(LORA_UART, uart_buffer_size, uart_buffer_size, 0, NULL, 0));

}

void device_uart_setup() {
    /* UART setup */
    uart_config_t hpm = {
        /* UART baud rate */            .baud_rate = 9600,
        /* UART byte size */            .data_bits = UART_DATA_8_BITS,
        /* UART parity mode */          .parity = UART_PARITY_DISABLE,
        /* UART stop bits */            .stop_bits = UART_STOP_BITS_1,
        /* UART HW flow control mode */ .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    DEBUG_PRINTF("Init multiplex uart.");
    ESP_ERROR_CHECK(uart_param_config(DEVS_UART, &hpm));
    gpio_set_direction(SW_SEL, GPIO_MODE_OUTPUT);

    //esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
    ESP_ERROR_CHECK(uart_set_pin(DEVS_UART, DEVS_UART_TX, DEVS_UART_RX, RS485_DE, UART_PIN_NO_CHANGE));

    const int uart_buffer_size = 2 * 1024;
    ESP_ERROR_CHECK(uart_driver_install(DEVS_UART, uart_buffer_size, uart_buffer_size, 0, NULL, 0));
}



void i2c_setup() {

    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2CPIN_MASTER_SDA;
    conf.scl_io_num = I2CPIN_MASTER_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 10000;

    if (i2c_param_config(I2CBUS, &conf) != ESP_OK) {
        ERROR_PRINTF("Error configuring I2C.");
        return;
    }

    if (i2c_driver_install(I2CBUS, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) {
        ERROR_PRINTF("Error setting I2C to master.");
        return;
    }
}

void app_main(void)
{
    notification("ENTRYPOINT REACHED");

    notification("CONFIGURING UARTS AND I2C");
    getmac();
    lora_uart_setup();
    device_uart_setup();
    adc_setup();
    i2c_setup();
    tsl_setup();
    hpm_setup();
    smart_meter_setup();
    volume_setup();


    printf("i\tfi\tf\tu\n");
    for(int i = 0; i < 2000; i++) {
        float fi = i;
        float f = sqrt(fi);
        uint16_t u = f;
        printf("%u\t%f\t%f\t\%u\n", i, fi, f, (unsigned int) u);
    }


    for(;;) {

        // announce these every once in a while
        while (mqtt_announce_str("sku", "ENV-01")) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        while (mqtt_announce_str("fw", GIT_COMMIT)) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        mqtt_announce_dropped();

        for(int i = 0; i < 100; i++) {
            // announce these more frequently
            status_led_toggle();

            hpm_query(); /* (Honeywell) particle meter */
            hdc_query(); /* humidity sensor with temperature sensor */
            tsl_query(); /* Lux/light sensor */

            smart_meter_query();
            water_volume_query();
            light_volume_query();

            for(int j = 0; j < 100; j++) {
                sound_query();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
   
        }
//        configure();
    }

    fflush(stdout);
    esp_restart();
}
