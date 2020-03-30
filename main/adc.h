#include <driver/adc.h>

void adc_setup();
//TODO: adc1_channel_t will be deprecated soon.
float adc_get(adc1_channel_t channel);
