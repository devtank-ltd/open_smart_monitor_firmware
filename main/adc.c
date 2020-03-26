#include <driver/adc.h>
#include "adc.h"
#include "pinmap.h"

void adc_setup() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);
}

float adc_get(adc1_channel_t channel) {

    printf("Gate = %d\n", gpio_get_level(SOUND_GATE));

    esp_err_t r = adc1_get_raw(channel);
    if(r < 0) {
        printf("An error occurred when querying the ADC.\n");
        return 0;
    }

    printf("Raw reading from ADC1: %d\n", r);
    return (r / 4096);
}
