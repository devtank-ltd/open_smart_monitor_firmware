#ifndef __HPM_H
#include "inttypes.h"
void hpm_uart_setup();
int hpm_query(uint16_t * pm25, uint16_t * pm10);
#define __HPM_H
#endif
