#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Because we want to work in integers for runtime cost reasons, but also want
 * to be able to report an approximation to a real number, we're going to
 * multiply the sample as it's enqueued, and then insert a decimal place at the
 * time that the number is reported, which is simply a matter of rendering
 * or formatting.
 *
 * The amount by which we multiply it is PRECISION. PRECISION can be any 
 * number, 1, 10, 100, 1000 etc. The number of zeroes is the number of decimal
 * places in the reported measurement.
 */
#define PRECISION 100 

#include "stdint.h"

// Because these tokens are used as strings for the NVS keys, none of them may
// be longer than 15 characters.
#define FOREACH_PARAM(FN) \
    FN(temperature) \
    FN(humidity) \
    FN(sound) \
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
    FN(pulse1) \
    FN(pulse2) \
    FN(temp_probe) \
    FN(battery_mv) \
    FN(battery_pc)


#define GENERATE_GETTER(ENUM) get_ ## ENUM,
#define DECLARE_GETTER(ENUM) void get_ ## ENUM ();
#define GENERATE_ENUM(ENUM) parameter_ ## ENUM,
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

// this macro will expand to void get_temperature();, void get_sound_level();, void get_humidity();, etc.
FOREACH_PARAM(DECLARE_GETTER);

static void (*parameter_getters[ARRAY_SIZE(parameter_names)])() __attribute__((unused)) = {
    // this macro will expand to get_temperature, get_sound_level, get_humidity, etc.
    FOREACH_PARAM(GENERATE_GETTER)
};

typedef struct {
    enum parameters parameter;
    int sample;
} sample_t;


void stats_init();
void stats_enqueue_sample(int parameter, int value);
