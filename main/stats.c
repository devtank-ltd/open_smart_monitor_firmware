#include "stats.h"

void stats(int32_t * arr, unsigned int len, int64_t * avg, int32_t * min, int32_t * max) {
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
