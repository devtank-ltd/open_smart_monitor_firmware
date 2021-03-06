#include <driver/adc.h>
#include <esp_timer.h>
#include "adc.h"
#include "pinmap.h"
#include "logging.h"
#include "mqtt.h"
#include "config.h"
#include "math.h"
#include "driver/i2s.h"
#include "stats.h"

static esp_timer_handle_t periodic_timer;
static void periodic_timer_callback(void* arg);


#define ADCBUFLEN 1024
static volatile uint16_t micvolts[ADC_AVG_SLOTS] = {0};
static volatile uint16_t bat_values[ADC_AVG_SLOTS] = {0};
static unsigned adc_values_index = 0;

#define AMP_GAIN 31.82
#define ADC_COUNT 4095
#define EXAMPLE_I2S_NUM 0
#define EXAMPLE_I2S_SAMPLE_RATE 22000
#define EXAMPLE_I2S_FORMAT        (I2S_CHANNEL_FMT_RIGHT_LEFT)

float midpoint = 0;

void adc_setup() {
    midpoint = get_midpoint();
    int i2s_num = EXAMPLE_I2S_NUM;
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX | I2S_MODE_ADC_BUILT_IN,
        .sample_rate =  EXAMPLE_I2S_SAMPLE_RATE,
        .bits_per_sample = 16,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .channel_format = EXAMPLE_I2S_FORMAT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 2,
        .dma_buf_len = ADCBUFLEN,
        .use_apll = 1,
    };
    //install and start i2s driver
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    //init ADC pad
    i2s_set_adc_mode(ADC_UNIT_1, SOUND_OUTPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SOUND_OUTPUT,   ADC_ATTEN_DB_11);

    adc2_config_channel_atten(BATMON,   ADC_ATTEN_DB_11);

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "adc_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, ADC_USECS_PER_SLOT));
}

static uint16_t adc2_safe_get(adc2_channel_t channel) {
    int r = 0;
    esp_err_t err = adc2_get_raw(channel, ADC_WIDTH_BIT_12, &r);
    ESP_ERROR_CHECK(err);
    return (uint16_t)r;
}


static void periodic_timer_callback(void* arg) {
    bat_values[adc_values_index] = adc2_safe_get(BAT_MON);
    adc_values_index += 1;
    adc_values_index %= ADC_AVG_SLOTS;
}

static double adc_avg_get(unsigned index) {
    int r = 0;
    for (unsigned n = 0; n < ADC_AVG_SLOTS; n++) {
        r += bat_values[n];
    }
    return r / (double)ADC_AVG_SLOTS;
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

int db_correction(int db) {
    if(db < 60) return 60;
    if(db < 66) return db + 5;
    if(db < 71) return db + 13;
    if(db < 99) return db + 15;
    return db + 18;
}

void get_battery_pc() {
    // FIXME: Too many magic numbers here.
    int v = voltagecalc(adc_avg_get(1)) * 3197;
    int pc = (v - 2500) / .17;
    if(pc < 0) pc = 0;
    if(pc > 100) pc = 100;
    mqtt_enqueue_int("battery_pc", NULL, pc);
}

void get_battery_mv() {
    // FIXME: Too many magic numbers here.
    int v = voltagecalc(adc_avg_get(1)) * 3197;
    mqtt_enqueue_int("battery_mv", NULL, v);
}

void get_sound() {
    long double vrms = 0;
    size_t bytes_read;
    i2s_read(EXAMPLE_I2S_NUM, (void*)micvolts, ADCBUFLEN, &bytes_read, portMAX_DELAY);
    unsigned int n;
    for (n = 0; n < bytes_read; n++) {
        uint16_t adcsample = micvolts[n] & 0x0fff;
        if(!adcsample) break;
        double a = voltagecalc(adcsample) - midpoint;
//        printf("%u\n", (unsigned int) adcsample);
        vrms += a * a;
    }
    vrms = sqrtl(vrms/n);

    // This equation 
    double db = db_correction((20*log10(vrms/0.00891))-AMP_GAIN+94);

    stats_enqueue_sample(parameter_sound, db * 10);
}

