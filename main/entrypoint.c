#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "driver/i2c.h"
#include "nvs_flash.h"
#include "uplink.h"
#include "mqtt-sn.h"
#include "mqtt-dbg.h"

#include "pinmap.h"

#include "mqtt.h"
#include "mac.h"
#include "config.h"
#include "status_led.h"
#include "logging.h"
#include "commit.h"
#include "math.h"
#include "stats.h"
#include "measurements.h"

#define LEDSTACKSIZE 1000
#define MEASSTACKSIZE 10000

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
    conf.master.clk_speed = 100000;
    conf.clk_flags = 0;

    esp_err_t err = i2c_param_config(I2CBUS, &conf);
    if (err != ESP_OK) {
        ERROR_PRINTF("Error (%s) configuring I2C.", esp_err_to_name(err));
        return;
    } else {
        DEBUG_PRINTF("(%s) configuring I2C.", esp_err_to_name(err));
    }

    err = i2c_driver_install(I2CBUS, I2C_MODE_MASTER, 0, 0, 0);
    if(err != ESP_OK) {
        ERROR_PRINTF("Error (%s) setting I2C to master.",esp_err_to_name(err));
        return;
    } else {
        DEBUG_PRINTF("(%s) configuring I2C.", esp_err_to_name(err));
    }
}

TaskHandle_t xLEDHandle = NULL;
StaticTask_t xLEDBuffer;
StackType_t  xLEDStack[LEDSTACKSIZE];

TaskHandle_t xMeasureHandle = NULL;
StaticTask_t xMeasureBuffer;
StackType_t  xMeasureStack[MEASSTACKSIZE];
/*
TaskHandle_t xMQTTHandle = NULL;
StaticTask_t xMQTTBuffer;
StackType_t  xMQTTStack[MEASSTACKSIZE];
*/
void app_main(void)
{
    nvs_flash_init();
    mqtt_init();
    stats_init();
    i2c_setup();
    getmac();
    lora_uart_setup();
    device_uart_setup();
    status_led_set_status(STATUS_LED_OK);
    uplink_init();

    if(get_mqtten() == MQTTSN_OVER_LORA)
        mqtt_sn_init();

    if(get_mqtten() == MQTT_LOG_TO_USB)
        mqtt_dbg_init();

    xLEDHandle = xTaskCreateStatic(
                      status_led_task, /* Function that implements the task. */
                      "LEDBLINK",      /* Text name for the task. */
                      LEDSTACKSIZE,    /* Number of indexes in the xStack array. */
                      (void*)1,        /* Parameter passed into the task. */
                      tskIDLE_PRIORITY,/* Priority at which the task is created. */
                      xLEDStack,       /* Array to use as the task's stack. */
                      &xLEDBuffer);    /* Variable to hold the task's data structure. */


    xMeasureHandle = xTaskCreateStatic(
                      measurements_task,
                      "MEASUREMENTS",
                      MEASSTACKSIZE,
                      (void*)1,
                      tskIDLE_PRIORITY + 1,
                      xMeasureStack,
                      &xMeasureBuffer);

    for(;;) vTaskDelay(INT_MAX);
    esp_restart();
}
