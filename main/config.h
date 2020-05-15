#pragma once
#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 50

#define ADC_AVG_SLOTS       32    /* How many samples to work on. */
#define ADC_MAX_AVG          8    /* How many max samples to average. */
#define ADC_USECS_PER_SLOT  10000 /* Together with the number of slots, it's 1/3 of a second before the slots wrap round.*/

void configure();
void store_config(char * key, char * val);
const char* get_config(const char * key);
