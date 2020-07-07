#include "stats.h"

void stats(uint16_t * arr, unsigned int len, uint16_t * avg, uint16_t * min, uint16_t * max) {
    *min = arr[0];
    *max = arr[0];
    *avg = 0;

    for(int i = 0; i < len; i++) {
        uint16_t samp = arr[i];
        if(*min > samp) *min = samp;
        if(*max < samp) *max = samp;
        *avg += samp;
    }
    *avg /= len;
}
