#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "stdint.h"

#define FOREACH_PARAM(FN) \
    FN(temperature) \
    FN(humidity) \
    FN(sound_level) \
    FN(pm10) \
    FN(pm25) \
    FN(light) \
    FN(current1) \
    FN(current2) \
    FN(current3) \
    FN(voltage1) \
    FN(voltage2) \
    FN(voltage3) \
    FN(powerfactor) \
    FN(pfleadlag) \
    FN(freq1) \
    FN(freq2) \
    FN(external_temperature)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,
#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

enum parameters {
    // this macro will expand to temperature, sound_level, humidity, etc.
    FOREACH_PARAM(GENERATE_ENUM)
};

static const char * parameter_names[] = {
    // this macro will expand to "temperature", "sound_level", "humidity", etc.
    FOREACH_PARAM(GENERATE_STRING)
};

extern xQueueHandle queues[ARRAY_SIZE(parameter_names)];

void stats_init();
