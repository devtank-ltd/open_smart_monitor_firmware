#include "driver/gpio.h"
#include "status_led.h"
#include "pinmap.h"
#include "logging.h"

void status_led_toggle() {
    static bool status;
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, status);
    INFO_PRINTF("Status LED %s", status ? "on" : "off");
    status = !status;
}
