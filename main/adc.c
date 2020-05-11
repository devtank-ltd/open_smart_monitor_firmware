#include <driver/adc.h>
#include "adc.h"
#include "pinmap.h"
#include "logging.h"
#include "mqtt-sn.h"

void adc_setup() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);
}


static int adc_get(adc1_channel_t channel) {

    DEBUG_PRINTF("Gate = %d", gpio_get_level(SOUND_GATE));

    int r = adc1_get_raw(channel);
    if(r < 0) {
        ERROR_PRINTF("An error occurred when querying the ADC.");
        return 0;
    }

    DEBUG_PRINTF("Raw reading from ADC1(%u): %d", (unsigned)channel, r);
    return (r * 1000) / 4095;
}


void sound_query() {
    int v = adc_get(SOUND_OUTPUT);
    INFO_PRINTF("Sound output : %d", v);
    mqtt_announce_int("Snd Level", (int)(v * 1000));
    v = adc_get(SOUND_ENVELOPE);
    INFO_PRINTF("Sound envelope : %d", v);
    mqtt_announce_int("Snd Envlp", (int)(v * 1000));
}
