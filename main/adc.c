#include <driver/adc.h>
#include <esp_timer.h>
#include "adc.h"
#include "pinmap.h"
#include "logging.h"
#include "mqtt-sn.h"
#include "config.h"

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
    adc_values[adc_values_index][0] = adc1_safe_get(SOUND_OUTPUT);
    adc_values[adc_values_index][1] = adc2_safe_get(BATMON);
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


static int adc_max_get(unsigned index) {
    unsigned tops[ADC_MAX_AVG] = {0};
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        unsigned v = adc_values[n][index];
        for (unsigned i = 0; i < ADC_MAX_AVG; i++) {
            if (!tops[i] || tops[i] < v)
                 tops[i] = v;
        }
    }
    int r = 0;
    for (unsigned n = 0; n < ADC_MAX_AVG; n++) {
        r += tops[n];
    }
    return (r * 10000) / ADC_MAX_AVG / 4095;
}


void sound_query() {
    int v = adc_max_get(0);
    INFO_PRINTF("Sound output : %d", v);
    mqtt_announce_int("SOUNDLEVEL", v);
    v = adc_avg_get(1);
    INFO_PRINTF("BATMON : %d", v);
    mqtt_announce_int("BATMON", v);
}
