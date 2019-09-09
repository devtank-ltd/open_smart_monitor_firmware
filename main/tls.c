#include "devices.h"

#include "driver/i2c.h"

#define TLS2591_ADDR  0x29

#define C0DATAL       0x14
#define C0DATAH       0x15
#define C1DATAL       0x16
#define C1DATAH       0x17

#define ENABLE        0x00

#define ENABLE_PON    0x01
#define ENABLE_AEN    0x02

#define STATUS        0x13
#define STATUS_AVALID 0x01


#define ACK_CHECK_EN  0x01
#define ACK_CHECK_DIS 0x00

uint8_t read_tls_reg(uint8_t reg) {
    uint8_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TLS2591_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    i2c_master_start(cmd);
    i2c_master_read_byte(cmd, &ret, ACK_CHECK_DIS);
    i2c_master_stop(cmd);

    return ret;
}

void write_tls_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TLS2591_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);

    // For the following byte, bits 101x xxxx means "select register xxxxx". This is specified in the data sheet on page 14.
    i2c_master_write_byte(cmd, 0xA0 | reg, ACK_CHECK_EN);
    // The following byte is the payload, or value that will be stored into the selected register.
    i2c_master_write_byte(cmd, value, ACK_CHECK_DIS);
    i2c_master_stop(cmd);
}

void tsl_powerdown() {
    write_tls_reg(ENABLE, 0x00);
}

void tsl_init() {
    write_tls_reg(ENABLE, ENABLE_PON | ENABLE_AEN);
}

void tsl_query(uint16_t * c0, uint16_t * c1) {

    tsl_init();

    // Wait for the AVALID bit to equal 1, which signals that an integration cycle has completed and
    // the channels are ready to be read. 
    while(!(read_tls_reg(STATUS) && STATUS_AVALID));

    // The datasheet recommends that all four bytes of ALS data are read as a single transaction to minimise skew between the two channels.
    // Some examples out there in the wild also read this data twice, due to the fact that the device is double-buffered. I don't know that that's really necessary though.
    uint8_t tmpl;
    uint8_t tmph;

    tmpl = read_tls_reg(C0DATAL);
    tmph = read_tls_reg(C0DATAH);
    *c0 = tmph * 256 + tmpl;

    tmpl = read_tls_reg(C1DATAL);
    tmph = read_tls_reg(C1DATAH);
    *c1 = tmph * 256 + tmpl;
    tsl_powerdown();
}
