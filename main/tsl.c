#include "devices.h"

#include "driver/i2c.h"

// possible addresses are: 0x29, 0x39, 0x49.
#define TSL2591_ADDR  0x39 // 0x39 is correct on the lashup.

#define C0DATAL       0x0c
#define C0DATAH       0x0d
#define C1DATAL       0x0e
#define C1DATAH       0x0f

#define COMMAND       0x80

#define CONTROL       0x00

#define CONTROL_ON    0x03
#define CONTROL_OFF   0x00

#define STATUS        0x13
#define STATUS_AVALID 0x01
#define TIMING        0x01

#define ACK_CHECK_EN  0x01
#define ACK_CHECK_DIS 0x00

uint8_t read_tsl_reg(uint8_t reg) {
    uint8_t ret = 0;
    esp_err_t err = ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, COMMAND | reg, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &ret, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK) printf("Trouble2 %s reading from the TSL2561\n", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);

    return ret;
}

void write_tsl_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);

    i2c_master_write_byte(cmd, COMMAND | reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK) printf("Trouble %s writing to the TSL2561\n", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
}

void tsl_powerdown() {
    write_tsl_reg(CONTROL, CONTROL_OFF);
}

void tsl_init() {
    write_tsl_reg(CONTROL, CONTROL_ON);
    write_tsl_reg(TIMING,  0x02); // An integration cycle begins every 402ms.
    printf("TSL2561 initialised %d \n", read_tsl_reg(CONTROL));
}

void tsl_query(uint16_t * c0, uint16_t * c1) {

    uint8_t alive = read_tsl_reg(CONTROL) & 0x03;
    if(alive != 0x03) {
        printf("TSL2561 was found to be dead. %u\n", alive);
    }

    // The datasheet recommends that all four bytes of ALS data are read as a single transaction to minimise skew between the two channels.
    // Some examples out there in the wild also read this data twice, due to the fact that the device is double-buffered. I don't know that that's really necessary though.
    uint8_t tmpl;
    uint8_t tmph;

    tmpl = read_tsl_reg(C0DATAL);
    tmph = read_tsl_reg(C0DATAH);
    *c0 = tmph * 256 + tmpl;
    printf("c0 = %u\n", *c0);
    tmpl = read_tsl_reg(C1DATAL);
    tmph = read_tsl_reg(C1DATAH);
    *c1 = tmph * 256 + tmpl;
//    tsl_powerdown();
}
