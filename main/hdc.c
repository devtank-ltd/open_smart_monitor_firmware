#include "devices.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define HDC2080_ADDR  0x40

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

uint8_t read_reg(uint8_t reg) {
    uint8_t ret = 0x5a;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &ret, ACK_CHECK_DIS);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK) printf("Trouble %s reading from the HDC2080\n", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);

    return ret;
}

void write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (HDC2080_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_DIS);
    i2c_master_stop(cmd);
    if(i2c_master_cmd_begin(I2CBUS, cmd, 50) != ESP_OK) printf("Trouble writing to the HDC2080\n");
    i2c_cmd_link_delete(cmd);
}

void hdc_init() {
    write_reg(CONFIG, MEAS_TRIG);
}

void hdc_wait() {
    int i;
    for(i = 50; !i; i--) {
        if(read_reg(CONFIG) && MEAS_TRIG) {
            printf("Waiting %d for HDC2080\n", i);
            vTaskDelay(500);
        } else {
            printf("HDC2080 reports being ready\n");
            return;
        }
    }
}

uint16_t q16(uint8_t reg_l, uint8_t reg_h) {
    return read_reg(reg_h) * 256 + read_reg(reg_l);
}

void hdc_query(float * temp_celsius, float * relative_humidity) {

//    hdc_wait();
    hdc_init();
    hdc_wait();
   
    uint16_t tempreading = q16(TMP_L, TMP_H);
    *temp_celsius = ((float) tempreading/65536.0) * 165 - 40; // Equation 1 in the HDC2080 datasheet
    
    uint16_t humreading = q16(HUM_L, HUM_H);
    *relative_humidity = ((float) humreading/65536.0) * 100; // Equation 2 in the HDC2080 datasheet
    printf(" raw readings from hdc2080 %d %d\n", tempreading, humreading);
}
