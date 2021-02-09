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
#define PULSEIN_UNUSED      0  // Don't use either pulse input
#define PULSEIN_1FREQ       1  // Use P1 as a frequency counter and leave P2 unused
#define PULSEIN_2FREQ       2  // Use both P1 and P2 as frequency counters 
#define PULSEIN_1FREQ1PULSE 3  // Use P1 as a frequency counter, P2 as a pulse counter
#define PULSEIN_1PULSE      4  // Use P1 as a pulse counter, leave P2 unused
#define PULSEIN_2PULSE      5  // Use both P1 and P2 as pules counters

#define MQTT_DISABLE        0
#define MQTTSN_OVER_LORA    1
#define MQTTSN_OVER_LORAWAN 2
#define MQTT3_OVER_WIFI     3

void configure();
void store_config(char * key, char * val);
const char* get_config(const char * key);


void set_midpoint(float v);
float get_midpoint();
void set_mqtten(uint8_t en);
uint8_t get_mqtten();
void set_socoen(uint8_t en);
uint8_t get_socoen();
void set_hpmen(uint8_t en);
uint8_t get_hpmen();
void set_pulsein(uint8_t en);
uint8_t get_pulsein();
uint32_t get_wateroffset();
void set_wateroffset(uint32_t);
