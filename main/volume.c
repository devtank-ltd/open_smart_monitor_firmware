#include "driver/gpio.h"
#include "pinmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 1000 microseconds is 1 millisecond.
#define DEBOUNCE_WAIT 1000

static int count1 = 0;
static int count2 = 0;

static inline int debounce(int64_t * old_time, int * old_level, int new_level) {
    // esp_timer_get_time returns in microseconds, the time since startup.
    // As that's an int64_t there is a slight risk of missing one pulse every 292471 years when this timer wraps
    volatile int64_t now = esp_timer_get_time();

    // If the transitions are too close together they should not be counted
    if((now <= previous_edge + DEBOUNCE_WAIT)) return 0;

    // If the current level is the same as the previous one, then the transition should not be counted
    // (This eliminates peaks during the high-to-low transition and troughs during the low-to-high transition
    if(*old_level == new_level) return 0;

    // Otherwise we'll count this as a genuine transition
    *old_time = now;
    *old_level = new_level;
    return 1;
}


static void IRAM_ATTR isr_p2(void * arg) {
    static int level;
    volatile static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_2);
    if(debounce(&previous_edge, &level, new_level) && new_level) count2++;
}

static void IRAM_ATTR isr_p1(void * arg) {
    static int level;
    volatile static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_1);
    if(debounce(&previous_edge, &level, new_level) && new_level) count1++;
}

void freq_report(void * arg) {
    static int old_count2 = 0;
    printf("%d Hz\n", count2 - old_count2);
    old_count2 = count2;
}

void volume_setup() {
    printf("Setting the volume measurement gpio up\n");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << PULSE_IN_1) | (1ULL << PULSE_IN_2);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &freq_report,
        .name = "freq_report"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // fire the timer every second

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_1, isr_p1, (void*) PULSE_IN_1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_2, isr_p2, (void*) PULSE_IN_2));
}

void qry_volume() {
    printf("volume = %d\n", count2);
//    printf("pulsecounts: %d, %d.\n", cnt2, cnt3);
}
