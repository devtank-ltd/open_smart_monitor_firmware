#include <stdint.h>

void volume_setup();
void hpm_uart_setup();
int hpm_query(uint16_t * pm25, uint16_t * pm10);
void tsl_init();
void tsl_query(uint16_t * c0, uint16_t * c1);

void hdc_query(float * temp_celsius, float * relative_humidity);

void init_smart_meter();
void query_countis();

void qry_volume();

#define I2CBUS I2C_NUM_0
