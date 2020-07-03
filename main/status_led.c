#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"
#include "logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void status_led_toggle() {
    static bool status;
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, status);
    status = !status;
}

void status_led_task(void *pvParameters) {
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    for(;;) {
        INFO_PRINTF("Status LED " "on");
        // Turn on the LED for a short time
        gpio_set_level(STATUS_LED, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Turn off the LED for a somewhat longer time
        INFO_PRINTF("Status LED " "off");
        gpio_set_level(STATUS_LED, 0);
        vTaskDelay(600 / portTICK_PERIOD_MS);

    }
}

