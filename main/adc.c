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

static volatile uint16_t micvolts[ADC_AVG_SLOTS] = {0};
static volatile uint16_t bat_values[ADC_AVG_SLOTS] = {0};
static unsigned adc_values_index = 0;

#define AMP_GAIN 40
#define CONST_DB_OFFS 40
#define ADC_COUNT 4095
#define MIDPOINT 1.63246

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
    int t = adc1_safe_get(SOUND_OUTPUT);
    micvolts[adc_values_index] = t;
    printf("%u\n", t);
//    adc_values[adc_values_index][1] = adc2_safe_get(BATMON);
    adc_values_index += 1;
    adc_values_index %= ADC_AVG_SLOTS;
}

static int adc_avg_get(unsigned index) {
    int r = 0;
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        r += micvolts[n];
    }
    return (r * 10000) / ADC_AVG_SLOTS / 4095;
}

void battery_query() {
    int v = adc_avg_get(1);
    INFO_PRINTF("BATMON : %d", v);
    mqtt_announce_int("BATMON", v);
}

double voltagecalc(int adc_count){
      if(adc_count < 1 || adc_count > 4095) return 0;
      return -6.20034e-27 * pow(adc_count, 8)\
             +1.09482e-22 * pow(adc_count, 7)\
             -7.95465e-19 * pow(adc_count, 6)\
             +3.06831e-15 * pow(adc_count, 5)\
             -6.76502e-12 * pow(adc_count, 4)\
             +8.52865e-09 * pow(adc_count, 3)\
             -5.76208e-06 * pow(adc_count, 2)\
             +0.00255141  * adc_count \
             -0.00165281;
}

void sound_query() {
    long double vrms = 0;
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        double a = voltagecalc(micvolts[n]) - MIDPOINT;
        vrms += a * a;
    }
    vrms = sqrtl(vrms);

    // This equation 
    double db = (20*log10(vrms/0.00891))-AMP_GAIN+94;

//    uint16_t old_db = 0;
//    uint16_t idb = db + CONST_DB_OFFS;
    uint16_t raw = adc1_safe_get(SOUND_OUTPUT);
    printf("vrms = %Lf\traw = %u\t", vrms, raw);
    printf("%fV\t", voltagecalc(raw));
    printf("%fdB\n", db);
//    mqtt_delta_announce_int("SOUNDLEVEL", &idb, &old_db, 1);
//    mqtt_announce_int("RawADC", adc1_safe_get(SOUND_OUTPUT));
}

