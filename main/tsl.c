#include "tsl.h"
#include "pinmap.h"
#include "driver/i2c.h"
#include "mqtt-sn.h"
#include "logging.h"
#include <math.h>
// possible addresses are: 0x29, 0x39, 0x49.
// The address selection pin may be:
// floating    - 0x29
// grounded    - 0x39
// tied to Vin - 0x49
#define TSL2591_ADDR  0x29 // 0x39 is correct on the lashup.

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

#define ABS(x)  (x<0)?-x:x
#define LUM_DELTA 4

static uint8_t read_tsl_reg(uint8_t reg) {
    uint8_t ret = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, COMMAND | reg, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &ret, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK) ERROR_PRINTF("Trouble %s reading from the TSL2561", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);

    return ret;
}

static void write_tsl_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TSL2591_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);

    i2c_master_write_byte(cmd, COMMAND | reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2CBUS, cmd, 100);
    if(err != ESP_OK) ERROR_PRINTF("Trouble %s writing to the TSL2561", esp_err_to_name(err));
    i2c_cmd_link_delete(cmd);
}

static void tsl_powerdown() {
    write_tsl_reg(CONTROL, CONTROL_OFF);
}

void tsl_setup() {
    DEBUG_PRINTF("Init TSL");
    write_tsl_reg(CONTROL, CONTROL_ON);
    write_tsl_reg(TIMING,  0x02); // An integration cycle begins every 402ms.
    INFO_PRINTF("TSL2561 initialised %d", read_tsl_reg(CONTROL));
}

#define CH_SCALE 10
#define LUX_SCALE 14
#define RATIO_SCALE 9

#define K1T 0x0040
#define B1T 0x01f2
#define M1T 0x01be

#define K2T 0x0080
#define B2T 0x0214
#define M2T 0x02d1

#define K3T 0x00c0
#define B3T 0x023f
#define M3T 0x037b

#define K4T 0x0100
#define B4T 0x0270
#define M4T 0x03fe

#define K5T 0x0138
#define B5T 0x016f
#define M5T 0x01fc

#define K6T 0x019a
#define B6T 0x00d2
#define M6T 0x00fb

#define K7T 0x029a
#define B7T 0x0018
#define M7T 0x0012

#define K8T 0x029a
#define B8T 0x0000
#define M8T 0x0000

float afn(float r) {
    if(r < 0.1) return 0.604;
    if(r < 0.2) return 0.449;
    if(r < 0.4) return 0.406;
    if(r < 0.53)return 0.727;
    return 0;
}

void CalculateLux(uint16_t ch0, uint16_t ch1)
{
//    unsigned long chScale = (1 << CH_SCALE); // no scaling
//    unsigned long channel1;
//    unsigned long channel0;

    // scale the channel values
//    channel0 = (ch0 * chScale) >> CH_SCALE;
//    channel1 = (ch1 * chScale) >> CH_SCALE;

    // find the ratio of the channel values (Channel1/Channel0)
//    unsigned long ratio1 = 0;
//    if (channel0 != 0) ratio1 = (channel1 << (RATIO_SCALE+1)) / channel0;
//    else return;

//    unsigned long ratio = (ratio1 + 1) >> 1;

//    unsigned int b, m;

//    if(ratio <= K1T)       {b=B1T; m=M1T;}
//    else if (ratio <= K2T) {b=B2T; m=M2T;}
//    else if (ratio <= K3T) {b=B3T; m=M3T;}
//    else if (ratio <= K4T) {b=B4T; m=M4T;}
//    else if (ratio <= K5T) {b=B5T; m=M5T;}
//    else if (ratio <= K6T) {b=B6T; m=M6T;}
//    else if (ratio <= K7T) {b=B7T; m=M7T;}
//    else if (ratio >  K8T) {b=B8T; m=M8T;}

    if(!ch0) return;
    float r = ch1/(float)ch0;
    float a = afn(r);
    int lux;
    if(r < 0.54) lux = ch0 * (a - 0.062 * powf((ch1/ch0), 1.4));
    else if(r < 0.61) lux = 0.224 * ch0 - 0.031 * ch1;
    else if(r < 0.80) lux = 0.0128 * ch0 - 0.0153 * ch1;
    else if(r < 1.30) lux = 0.00146 * ch0 - 0.00112 * ch1;
    else lux = 0;


//    unsigned long temp;
//    temp = ((channel0 * b) - (channel1 * m));

//    temp += (1 << (LUX_SCALE - 1));
//    unsigned long lux = temp >> LUX_SCALE;

    uint16_t vis = lux;
    static uint16_t old_vis;

    printf("ch0 = %u, ch1 = %u, r = %f, a = %f\n", ch0, ch1, r, a);
    mqtt_delta_announce_int("VisibleLight", &vis, &old_vis, LUM_DELTA);

}



void tsl_query() {
    uint16_t c0;
    uint16_t c1;
    uint8_t alive = read_tsl_reg(CONTROL) & 0x03;
    if(alive != 0x03) {
        ERROR_PRINTF("TSL2561 was found to be dead. %u", alive);
    }

    // The datasheet recommends that all four bytes of ALS data are read as a single transaction to minimise skew between the two channels.
    // Some examples out there in the wild also read this data twice, due to the fact that the device is double-buffered. I don't know that that's really necessary though.
    uint8_t tmpl;
    uint8_t tmph;

    tmpl = read_tsl_reg(C0DATAL);
    tmph = read_tsl_reg(C0DATAH);
    c0 = tmph * 256 + tmpl;
    tmpl = read_tsl_reg(C1DATAL);
    tmph = read_tsl_reg(C1DATAH);
    c1 = tmph * 256 + tmpl;

    CalculateLux(c0, c1);
//    tsl_powerdown();
}
