/*
A driver for a DS18B20 thermometer by Devtank Ltd.

Documents used:
- DS18B20 Programmable Resolution 1-Wire Digital Thermometer
    : https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf                          (Accessed: 25.03.21)
- Understanding and Using Cyclic Redunancy Checks With maxim 1-Wire and iButton products
    : https://maximintegrated.com/en/design/technical-documents/app-notes/2/27.html     (Accessed: 25.03.21)
*/
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "pinmap.h"
#include "logging.h"
#include "stats.h"

#define DELAY_READ_START    2
#define DELAY_READ_WAIT     10
#define DELAY_READ_RELEASE  50
#define DELAY_WRITE_1_START 6
#define DELAY_WRITE_1_END   64
#define DELAY_WRITE_0_START 60
#define DELAY_WRITE_0_END   10
#define DELAY_RESET_SET     500
#define DELAY_RESET_WAIT    60
#define DELAY_RESET_READ    480
#define DELAY_GET_TEMP      750000

#define W1_CMD_SKIP_ROM     0xCC
#define W1_CMD_CONV_T       0x44
#define W1_CMD_READ_SCP     0xBE

typedef union {
    struct {
        uint16_t t;
        uint16_t tmp_t;
        uint8_t conf;
        uint8_t res0;
        uint8_t res1;
        uint8_t res2;
        uint8_t crc;
    } __attribute__((packed));
    uint8_t raw[9];
} w1_memory_t;

static int w1_read_bit(void) {
    portDISABLE_INTERRUPTS();
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OW_GPIO, 0);
    ets_delay_us(DELAY_READ_START);
    gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(DELAY_READ_WAIT);
    int out = gpio_get_level(OW_GPIO);
    ets_delay_us(DELAY_READ_RELEASE);
    portENABLE_INTERRUPTS();
    return out;
}

static int w1_send_bit(int bit) {
    portDISABLE_INTERRUPTS();
    if (bit) {
        gpio_set_level(OW_GPIO, 0);
        ets_delay_us(DELAY_WRITE_1_START);
        gpio_set_level(OW_GPIO, 1);
        ets_delay_us(DELAY_WRITE_1_END);
    } else {
        gpio_set_level(OW_GPIO, 0);
        ets_delay_us(DELAY_WRITE_0_START);                
        gpio_set_level(OW_GPIO, 1);                        
        ets_delay_us(DELAY_WRITE_0_END); 
    }
    portENABLE_INTERRUPTS();
    return 0;
}

static int w1_reset(void) {
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OW_GPIO, 0);
    ets_delay_us(DELAY_RESET_SET);
    gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(DELAY_RESET_WAIT);
    if (gpio_get_level(OW_GPIO) == 1) {
        return 1; 
        }
    ets_delay_us(DELAY_RESET_READ);
    return !(gpio_get_level(OW_GPIO));
}

static uint8_t w1_read_byte(void) {
    int val = 0;
    for (unsigned i = 0; i < 8; i++) {
        val = val + (w1_read_bit() << i);
    }
    return val;
}

static int w1_send_byte(uint8_t byte) {
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    for (unsigned i = 0; i < 8; i++) {
        w1_send_bit((byte & (1 << i)));
    }
    gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
    return 0;
}

void w1_read_scpad(w1_memory_t* d) {
    for (int i = 0; i < 9; i++) {
        d->raw[i] = w1_read_byte();
    }
}

int w1_crc_check(uint8_t* mem, uint8_t size) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < size; i++) {
         uint8_t byte = mem[i];
         for (uint8_t j = 0; j < 8; ++j) {
            uint8_t blend = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (blend) {
                crc ^= 0x8C;
            }
            byte >>= 1;
         }
    }
    return !(crc == mem[size]);
}

static void w1_temp_err(void) {
    DEBUG_PRINTF("Temperature probe did not respond");
}

void get_temp_probe(void) {
    float t_out;
    w1_memory_t d; 
    if (w1_reset()) {
        w1_temp_err();
        return;
    }
    w1_send_byte(W1_CMD_SKIP_ROM);                          
    w1_send_byte(W1_CMD_CONV_T);                        
    ets_delay_us(DELAY_GET_TEMP);
    if (w1_reset()) {
        w1_temp_err();
        return;
    }
    w1_send_byte(W1_CMD_SKIP_ROM);          
    w1_send_byte(W1_CMD_READ_SCP);                  
    w1_read_scpad(&d);
    
    if (w1_crc_check(d.raw, 8)) {
        DEBUG_PRINTF("Temperature data not confirmed by CRC");
        return;
    }

    if (d.t > 0xfc00) {                                 
        d.t = - d.t + 0xf920;
    }
    t_out = d.t / 16.0f;    
    DEBUG_PRINTF("T: %f C", t_out);
    stats_enqueue_sample(parameter_temp_probe, t_out);
}

int temp_init(void) {
    gpio_pad_select_gpio(OW_GPIO);
    if (w1_reset()) {
        w1_temp_err();
        return 1;
    }
    return 0;
}
