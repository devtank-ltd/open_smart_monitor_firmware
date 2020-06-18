#include <driver/adc.h>
#include <esp_timer.h>
#include "adc.h"
#include "pinmap.h"
#include "logging.h"
#include "mqtt-sn.h"
#include "config.h"
#include "math.h"
static esp_timer_handle_t periodic_timer;
static void periodic_timer_callback(void* arg);

static volatile uint16_t adc_values[ADC_AVG_SLOTS][2] = {0};
static unsigned adc_values_index = 0;


void adc_setup() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SOUND_OUTPUT,   ADC_ATTEN_DB_11);

    adc2_config_channel_atten(BATMON,   ADC_ATTEN_DB_11);

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, ADC_USECS_PER_SLOT));
}

static uint16_t adc1_safe_get(adc1_channel_t channel) {
    int r = adc1_get_raw(channel);
    if(r < 0) {
        ERROR_PRINTF("An error occurred when querying the ADC.");
        return 0;
    }
    return (uint16_t)r;
}

static uint16_t adc2_safe_get(adc2_channel_t channel) {
    int r = 0;
    esp_err_t err = adc2_get_raw(channel, ADC_WIDTH_BIT_12, &r);
    ESP_ERROR_CHECK(err);
    return (uint16_t)r;
}


static void periodic_timer_callback(void* arg) {
    int t = adc1_safe_get(SOUND_OUTPUT) - 2048; // Remove DC offset
    if(t < 0) t = -t;                           // Absolute value
    adc_values[adc_values_index][0] = (t * t);
//    adc_values[adc_values_index][1] = adc2_safe_get(BATMON);
    adc_values_index += 1;
    adc_values_index %= ADC_AVG_SLOTS;
}

static int adc_avg_get(unsigned index) {
    int r = 0;
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        r += adc_values[n][index];
    }
    return (r * 10000) / ADC_AVG_SLOTS / 4095;
}

void battery_query() {
    int v = adc_avg_get(1);
    INFO_PRINTF("BATMON : %d", v);
    mqtt_announce_int("BATMON", v);
}

void sound_query() {
    unsigned long vrms = 0;
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        vrms += adc_values[n][0];
    }
    vrms = sqrt(vrms / ADC_AVG_SLOTS);

    // This equation 
    float db = (20*log(vrms/8.9125*.001))-34+94;

    uint16_t old_db = 0;
    uint16_t idb = db;
    mqtt_delta_announce_int("SOUNDLEVEL", &idb, &old_db, 1);
    mqtt_announce_int("RawADC", adc1_safe_get(SOUND_OUTPUT));
}

