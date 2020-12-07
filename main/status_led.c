#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"
#include "logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

volatile static int led_status = STATUS_LED_OK;

void status_led_set_status(int status) {
    led_status = status;
}

void status_led_toggle() {
    static bool status;
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, status);
    status = !status;
}

void status_led_task(void *pvParameters) {
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    for(;;) {
        // Turn on the LED for a short time
        gpio_set_level(STATUS_LED, led_status == STATUS_LED_OK);
        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Turn off the LED for a somewhat longer time
        gpio_set_level(STATUS_LED, led_status != STATUS_LED_OK);
        vTaskDelay(600 / portTICK_PERIOD_MS);

    }
}

