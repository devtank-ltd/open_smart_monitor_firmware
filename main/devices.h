#include <stdint.h>


void hpm_uart_setup();
int hpm_query(uint16_t * pm25, uint16_t * pm10);

void tsl_query(uint16_t * c0, uint16_t * c1);
