#include <driver/adc.h>
#include "adc.h"
#include "pinmap.h"
#include "logging.h"

void adc_setup() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);
}


static float adc_get(adc1_channel_t channel) {

    DEBUG_PRINTF("Gate = %d", gpio_get_level(SOUND_GATE));

    int r = adc1_get_raw(channel);
    if(r < 0) {
        ERROR_PRINTF("An error occurred when querying the ADC.");
        return 0;
    }

    DEBUG_PRINTF("Raw reading from ADC1(%u): %d", (unsigned)channel, r);
    return (r / 4095.0f);
}


void sound_output_query() {
    float v = adc_get(SOUND_OUTPUT);
    INFO_PRINTF("Sound output : %G", v);
}


void sound_envelope_query() {
    float v = adc_get(SOUND_ENVELOPE);
    INFO_PRINTF("Sound envelope : %G", v);
}
