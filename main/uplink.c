//#include "stats.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "pinmap.h"
//#include "mqtt.h"
#include "string.h"
#include "logging.h"
#include "scpi.h"

#define LINELENGTH 80
#define QUEUESIZE 50
#define STACKSIZE 10000

TaskHandle_t xUplinkHandle = NULL;
StaticTask_t xUplinkBuffer;
StackType_t  xUplinkStack[STACKSIZE];

bool beginswith(const char * haystack, const char * needle, int * argument) {
    int length = strlen(needle);
    if(!strncmp(haystack, needle, length)) {
        *argument = atoi(haystack + length);
        return true;
    }
    return false;
}

void uplink_task(void *pvParameters) {

    for(;;) {
        char line[LINELENGTH];
        int received = uart_read_bytes(UPLINK_UART, line, LINELENGTH, 2000 / portTICK_PERIOD_MS);
        if(!received) {
            continue;
        }
        if(!strstr(line, "\r")) {
            continue;
        }

        int argument;

        if(beginswith(line, "set midpoint ", &argument)) {
            set_midpoint(argument);
            continue;
        }

        scpi_parse(line);

    }
}

void uplink_init() {
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };
    const int uart_buffer_size = 2 * 1024;
    ESP_ERROR_CHECK(uart_param_config(UPLINK_UART, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UPLINK_UART, uart_buffer_size, uart_buffer_size, 0, NULL, 0));

    xUplinkHandle = xTaskCreateStatic(
                      uplink_task,     /* Function that implements the task. */
                      "UPLINKTASK",    /* Text name for the task. */
                      STACKSIZE,       /* Number of indexes in the xStack array. */
                      (void*)1,        /* Parameter passed into the task. */
                      tskIDLE_PRIORITY,/* Priority at which the task is created. */
                      xUplinkStack,    /* Array to use as the task's stack. */
                      &xUplinkBuffer); /* Variable to hold the task's data structure. */

}
