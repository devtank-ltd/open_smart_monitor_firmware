#include "driver/gpio.h"
#include "pinmap.h"
#include "mqtt.h"
#include "stats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "logging.h"
#include "config.h"

// 1000 microseconds is 1 millisecond.
#define DEBOUNCE_WAIT 1000

static int setup1;
static int setup2;

static volatile int pin_count1 = 0;
static volatile int pin_freq1 = 0;
static volatile int pin_count2 = 0;
static volatile int pin_freq2 = 0;

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


static void IRAM_ATTR isr_p1(void * arg) {
    static int level;
    static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_1);
    if(debounce(&previous_edge, &level, new_level) && new_level) pin_count1++;
}

static void IRAM_ATTR isr_p2(void * arg) {
    static int level;
    static int64_t previous_edge = 0;
    int new_level = gpio_get_level(PULSE_IN_2);
    if(debounce(&previous_edge, &level, new_level) && new_level) pin_count2++;
}

static void IRAM_ATTR isr_p3(void * arg) {
}

static void freq_compute1(void * arg) {
    // Have this function run every second, and it will compute the
    // frequency of both pulses. In hertz.
    static int old_count1 = 0;
    pin_freq1 = pin_count1 - old_count1;
    old_count1 = pin_count1;
}

static void freq_compute2(void * arg) {
    // Have this function run every second, and it will compute the
    // frequency of both pulses. In hertz.
    static int old_count2 = 0;
    pin_freq2 = pin_count2 - old_count2;
    old_count2 = pin_count2;
}

static int freq_get1() {
    return pin_freq1;
}

static int freq_get2() {
    return pin_freq2;
}

static int pulse_get1() {
    return pin_count1;
}

static int pulse_count() {
    return pin_freq2;
}

void volume_setup() {
    int setup1 = get_pulsein1();
    int setup2 = get_pulsein2();
    DEBUG_PRINTF("Setting the volume measurement gpio up");
    gpio_config_t io_conf = {
        .intr_type = GPIO_PIN_INTR_ANYEDGE,
        .pin_bit_mask = (1ULL << PULSE_IN_1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };

    if(setup1 == PULSEIN_FREQ) {
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &freq_compute1,
            .name = "freq_compute1",
        };
        esp_timer_handle_t periodic_timer;
        ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // fire the timer every second
    }

    if(setup2 == PULSEIN_FREQ) {
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &freq_compute2,
            .name = "freq_compute2",
        };
        esp_timer_handle_t periodic_timer;
        ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // fire the timer every second
    }

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    if(setup1 != PULSEIN_UNUSED) ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_1, isr_p1, (void*) PULSE_IN_1));
    if(setup2 != PULSEIN_UNUSED) ESP_ERROR_CHECK(gpio_isr_handler_add(PULSE_IN_2, isr_p2, (void*) PULSE_IN_2));
    ESP_ERROR_CHECK(gpio_isr_handler_add(POWER_INT,  isr_p3, (void*) POWER_INT));
}

void query_pulsecount(const char * key, int multiplier, int which) {
    if(setup1 == PULSEIN_FREQ) stats_enqueue_sample(pulse1, freq1);
    if(setup2 == PULSEIN_FREQ) stats_enqueue_sample(pulse2, freq2);

    if(setup1 == PULSEIN_PULSE) mqtt_enqueue_int(parameter_names[pulse1], NULL, pin_count1);
    if(setup2 == PULSEIN_PULSE) mqtt_enqueue_int(parameter_names[pulse2], NULL, pin_count2);
}
