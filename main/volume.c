#include "driver/gpio.h"
#include "pinmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 1000 microseconds is 1 millisecond.
#define DEBOUNCE_WAIT 1000

static int litres = 0;
static int cnt2 = 0;
static int cnt3 = 0;

static void IRAM_ATTR isr_water(void * arg) {
    // This pulse input is the only one I've actually made an effort to debounce, since I don't know exactly the characteristics of the other two.
    static int lvl;
    volatile static int64_t previous_edge = 0;

    // esp_timer_get_time returns in microseconds, the time since startup.
    // As that's an int64_t there is a slight risk of missing one pulse every 292471 years when this timer wraps
    volatile int64_t now = esp_timer_get_time();

    // If the transitions are too close together they should not be counted
    if((now <= previous_edge + DEBOUNCE_WAIT)) return;

    // If the current level is the same as the previous one, then the transition should not be counted
    // (This eliminates peaks during the high-to-low transition and troughs during the low-to-high transition
    int nlvl = gpio_get_level(FLOWRATE_GPIO);
    if(lvl == nlvl) return;

    // We are counting this edge as being a genuine transition.
    previous_edge = now;
    lvl = nlvl;
    
    // If this edge was a falling one, then the count has gone up by one.
    if(lvl) litres++;
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
//    printf("pulsecounts: %d, %d.\n", cnt2, cnt3);
}
