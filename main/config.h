#pragma once
#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 50

#define ADC_AVG_SLOTS       1000  /* How many samples to work on. */
#define ADC_MAX_AVG          8    /* How many max samples to average. */
#define ADC_USECS_PER_SLOT  1000    /* Together with the number of slots, it's 1/3 of a second before the slots wrap round.*/

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
uint32_t get_wateroffset();
