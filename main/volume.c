#include "driver/gpio.h"
#include "pinmap.h"
#include "mqtt-sn.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "logging.h"

// 1000 microseconds is 1 millisecond.
#define DEBOUNCE_WAIT 1000

static volatile int count1 = 0;
static volatile int count2 = 0;
static volatile int freq1 = 0;
static volatile int freq2 = 0;


static inline int debounce(int64_t * old_time, int * old_level, int new_level) {
    // esp_timer_get_time returns in microseconds, the time since startup.
    // As that's an int64_t there is a slight risk of missing one pulse every 292471 years when this timer wraps
    volatile int64_t now = esp_timer_get_time();

    // If the transitions are too close together they should not be counted
    if((now <= *old_time + DEBOUNCE_WAIT)) return 0;

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
    static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_2);
    if(debounce(&previous_edge, &level, new_level) && new_level) count2++;
}

static void IRAM_ATTR isr_p1(void * arg) {
    static int level;
    static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_1);
    if(debounce(&previous_edge, &level, new_level) && new_level) count1++;
}

static void IRAM_ATTR isr_p3(void * arg) {
}

static void freq_compute(void * arg) {
    // Have this function run every second, and it will compute the
    // frequency of both pulses. In hertz.
    static int old_count1 = 0;
    static int old_count2 = 0;
    freq1 = count1 - old_count1;
    freq2 = count2 - old_count2;
    old_count1 = count1;
    old_count2 = count2;
}

void volume_setup() {
    DEBUG_PRINTF("Setting the volume measurement gpio up");
    gpio_config_t io_conf = {
        .intr_type = GPIO_PIN_INTR_ANYEDGE,
        .pin_bit_mask = (1ULL << PULSE_IN_1) | (1ULL << PULSE_IN_2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &freq_compute,
        .name = "freq_report"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // fire the timer every second

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_1, isr_p1, (void*) PULSE_IN_1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_2, isr_p2, (void*) PULSE_IN_2));
    ESP_ERROR_CHECK(gpio_isr_handler_add(POWER_INT,  isr_p3, (void*) POWER_INT));
}
/*
static void qry_frequency(const char * key, int which) {
    mqtt_announce_int(key, (which ? freq1 : freq2));
}
*/
static void qry_pulsecount(const char * key, int multiplier, int which) {
    mqtt_announce_int(key, (which ? count1 : count2) * multiplier);
}

void water_volume_query() {
    qry_pulsecount("WaterMeter", 10, 0);
}

void light_volume_query() {
    qry_pulsecount("Light", 10, 1);
}
