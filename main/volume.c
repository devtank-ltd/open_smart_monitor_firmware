#include "driver/gpio.h"
#include "pinmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static int litres = 0;
static int cnt2 = 0;
static int cnt3 = 0;

static void IRAM_ATTR isr_water(void * arg) {
    static int lvl;
    if(lvl != gpio_get_level(FLOWRATE_GPIO)) {
        lvl = !lvl;
        litres += 10;
    }
}

static void IRAM_ATTR isr_p1(void * arg) {
    static int lvl;
    if(lvl != gpio_get_level(PULSE_IN_1)) {
        lvl = !lvl;
        cnt2++;
    }
}

static void IRAM_ATTR isr_p2(void * arg) {
    static int lvl;
    if(lvl != gpio_get_level(PULSE_IN_2)) {
        lvl = !lvl;
        cnt3++;
    }
}

void volume_setup() {
    printf("Setting the volume measurement gpio up\n");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << FLOWRATE_GPIO) | (1ULL << PULSE_IN_1) | (1ULL << PULSE_IN_2);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(FLOWRATE_GPIO, isr_water, (void*)FLOWRATE_GPIO));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_1, isr_p1, (void*) PULSE_IN_1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_2, isr_p2, (void*) PULSE_IN_2));
}

void qry_volume() {
    printf("volume = %d\n", litres);
    printf("pulsecounts: %d, %d.\n", cnt2, cnt3);
}
