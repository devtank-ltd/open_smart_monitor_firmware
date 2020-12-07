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
bool hdc_samples_ready = false;

int32_t temperature[SAMPLES] = {0};
int32_t humidity[SAMPLES] = {0};
static int sample_no = 0;

typedef union {
    uint16_t d;
    struct {
        uint8_t l;
        uint8_t h;
    };
} unit_entry_t;

void hdcsample(uint16_t temp, uint16_t hum) {
    temperature[sample_no] = temp;
    humidity[sample_no] = hum;
    sample_no++;
    if(sample_no >= SAMPLES) {
        stats(temperature, SAMPLES, &mqtt_temperature_stats);
        stats(humidity, SAMPLES, &mqtt_humidity_stats);
    }
    sample_no %= SAMPLES;
}

static esp_err_t read_reg(uint8_t reg, uint8_t * val) {
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
    if(err != ESP_OK) {
        ERROR_PRINTF("Trouble %s reading from the HDC2080", esp_err_to_name(err));
        i2c_cmd_link_delete(cmd);
        return err;
    }
    i2c_cmd_link_delete(cmd);

    *val = ret;
    return ESP_OK;
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
    mqtt_stats_update_delta(&mqtt_humidity_stats, 30);
    mqtt_stats_update_delta(&mqtt_temperature_stats, 15);
}

static void hdc_wait() {
    int i;
    for(i = 50; !i; i--) {
        uint8_t config;
        esp_err_t err = read_reg(CONFIG, &config);

        if(err != ESP_OK) {
            vTaskDelay(500);
        } else if (config && MEAS_TRIG) {
            INFO_PRINTF("Waiting %d for HDC2080", i);
            vTaskDelay(500);
        } else {
            INFO_PRINTF("HDC2080 reports being ready");
            return;
        }
    }
}

static uint16_t q16(uint8_t reg_l, uint8_t reg_h, unit_entry_t * entry) {
    if(read_reg(reg_h, &entry->h) != ESP_OK) return ESP_FAIL;
    return read_reg(reg_l, &entry->l);
}

void hdc_query() {

    hdc_init();
    hdc_wait();

    unit_entry_t temp;
    unit_entry_t hum;

    if(q16(TMP_L, TMP_H, &temp) != ESP_OK) return;
    int32_t temp_celsius = (((int64_t)temp.d * 1000000 / (1 << 16)) * 165 / 100000) - 400;

    if(q16(HUM_L, HUM_H, &hum) != ESP_OK) return;
    int32_t relative_humidity = (((int64_t)hum.d * 1000000 / (1 << 16)) * 100 / 100000);

    if(temp_celsius > -300) {
        hdcsample(temp_celsius - 30, (relative_humidity * 10) + 30);
    } else {
        ERROR_PRINTF("I refuse to believe it's less than -30 degrees here.");
    }

}
