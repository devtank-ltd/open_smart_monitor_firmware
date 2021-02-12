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

typedef union {
    uint16_t d;
    struct {
        uint8_t l;
        uint8_t h;
    };
} unit_entry_t;

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
    i2c_master_cmd_begin(I2CBUS, cmd, 50);
    i2c_cmd_link_delete(cmd);
}

static void hdc_init() {
    write_reg(CONFIG, MEAS_TRIG);
}

static void hdc_wait() {
    int i;
    for(i = 50; !i; i--) {
        uint8_t config;
        esp_err_t err = read_reg(CONFIG, &config);

        if(err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(5));
        } else if (config && MEAS_TRIG) {
            INFO_PRINTF("Waiting %d for HDC2080", i);
            vTaskDelay(pdMS_TO_TICKS(5));
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

    stats_enqueue_sample(humidity, relative_humidity);
    if(temp_celsius > -300) {
        stats_enqueue_sample(temperature, temp_celsius);
    } else {
        ERROR_PRINTF("I refuse to believe it's less than -30 degrees here.");
    }

}
