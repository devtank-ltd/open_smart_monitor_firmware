#pragma once
#include <stdint.h>
#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 50

#define ADC_AVG_SLOTS       1000  /* How many samples to work on. */
#define ADC_MAX_AVG          8    /* How many max samples to average. */
#define ADC_USECS_PER_SLOT  1000    /* Together with the number of slots, it's 1/3 of a second before the slots wrap round.*/

/* This hardware has two pulse inputs, let's call them P1 and P2;
 * These may each be configured as either a frequency counter or a pulse counter.
 * Use pulse counters for things like gas meters and turnstiles
 * Use frequency counters for things like speedometers and whatnot
 */
#define PULSEIN_UNUSED      0  // Don't use the pulse input
#define PULSEIN_FREQ        1  // Use the pulse input as a frequency counter
#define PULSEIN_PULSE       2  // Use the pulse input as a pulse counter

#define MQTT_DISABLE        0
#define MQTTSN_OVER_LORA    1
#define MQTTSN_OVER_LORAWAN 2
#define MQTT3_OVER_WIFI     3
#define MQTT_LOG_TO_USB     4

void configure();

void set_midpoint(float v);
float get_midpoint();
void set_mqtten(uint8_t en);
uint8_t get_mqtten();
void set_socoen(uint8_t en);
uint8_t get_socoen();
void set_hpmen(uint8_t en);
uint8_t get_hpmen();
void set_pulsein1(uint8_t en);
uint8_t get_pulsein1();
void set_pulsein2(uint8_t en);
uint8_t get_pulsein2();
uint32_t get_wateroffset();
void set_wateroffset(uint32_t);
uint32_t get_timedelta(int parameter);
void set_timedelta(int parameter, uint32_t delta);
void set_sample_rate(int parameter, uint32_t delta);
uint32_t get_sample_rate(int parameter);
void config_init();
