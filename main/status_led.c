#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"

void status_led_toggle() {
    static bool status;
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, status);
    printf("Status LED %s\n", status ? "on" : "off");
    status = !status;
}
