#include "hdc.h"
#include "pinmap.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "mqtt-sn.h"
#include "stats.h"
#include "logging.h"

#define HDC2080_ADDR  0x41

#define TMP_L         0x00
#define TMP_H         0x01
#define HUM_L         0x02
#define HUM_H         0x03
#define CONFIG        0x0f

#define ACK_CHECK_EN  0x01
#define ACK_CHECK_DIS 0x00

/* Bit in the Measurement Configuration register, which
 * when set by master starts measurement, and which is reset
 * by the slave when measurement has been completed.
 */
#define MEAS_TRIG     0x01

#define ABS(X)  (X<0)?-X:X
#define TEMP_DELTA 2
#define RH_DELTA   5

#define SAMPLES 1000

uint16_t temperature[SAMPLES] = {0};
uint16_t humidity[SAMPLES] = {0};

void hdcsample(uint16_t temp, uint16_t hum) {
    static int sample_no = 0;
    temperature[sample_no] = temp;
    humidity[sample_no] = hum;
    sample_no++;
    sample_no %= SAMPLES;
}

void hdc_announce() {
    uint16_t temp_max = 0;
    uint16_t temp_min = 0;
    uint64_t temp_avg = 0;
    
    uint16_t hum_max = 0;
    uint16_t hum_min = 0;
    uint64_t hum_avg = 0;

    stats(temperature, SAMPLES, &temp_avg, &temp_min, &temp_max);
    stats(humidity, SAMPLES, &hum_avg, &hum_min, &hum_max);

    mqtt_announce_int("temperature-avg", temp_avg);
    mqtt_announce_int("temperature-min", temp_min);
    mqtt_announce_int("temperature-max", temp_max);

    mqtt_announce_int("humidity-avg", hum_avg);
    mqtt_announce_int("humidity-min", hum_min);
    mqtt_announce_int("humidity-max", hum_max);


}

static uint8_t read_reg(uint8_t reg) {
    uint8_t ret = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &ret, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK)
        ERROR_PRINTF("Trouble %s reading from the HDC2080", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);

    return ret;
}

static void write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_DIS);
    i2c_master_stop(cmd);
    if(i2c_master_cmd_begin(I2CBUS, cmd, 50) != ESP_OK)
        ERROR_PRINTF("Trouble writing to the HDC2080");
    i2c_cmd_link_delete(cmd);
}

static void hdc_init() {
    write_reg(CONFIG, MEAS_TRIG);
}

static void hdc_wait() {
    int i;
    for(i = 50; !i; i--) {
        if(read_reg(CONFIG) && MEAS_TRIG) {
            INFO_PRINTF("Waiting %d for HDC2080", i);
            vTaskDelay(500);
        } else {
            INFO_PRINTF("HDC2080 reports being ready");
            return;
        }
    }
}

static uint16_t q16(uint8_t reg_l, uint8_t reg_h) {
    return read_reg(reg_h) * 256 + read_reg(reg_l);
}

void hdc_query() {

    float temp_celsius, relative_humidity;


    hdc_init();
    hdc_wait();

    uint16_t tempreading = q16(TMP_L, TMP_H);
    temp_celsius = ((float) tempreading/65536.0) * 165 - 40; // Equation 1 in the HDC2080 datasheet

    uint16_t humreading = q16(HUM_L, HUM_H);
    relative_humidity = ((float) humreading/65536.0) * 100; // Equation 2 in the HDC2080 datasheet

    if(temp_celsius > -39)
        hdcsample(temp_celsius * 10, relative_humidity * 10);

}
