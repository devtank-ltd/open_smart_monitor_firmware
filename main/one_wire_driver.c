#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "pinmap.h"
#include "logging.h"

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
    uint16_t w;
    struct {
        uint8_t l;
        uint8_t h;
    };
} w1_data_t;

typedef struct { 
    w1_data_t t;
    w1_data_t t_tmp;
    uint8_t conf;
    uint8_t res0;
    uint8_t res1;
    uint8_t res2;
    uint8_t crc;
} w1_memory_t;

static int w1_read_bit(void) {
    portDISABLE_INTERRUPTS();
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(DELAY_READ_START);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(DELAY_READ_WAIT);
    int out = gpio_get_level(DS_GPIO);
    ets_delay_us(DELAY_READ_RELEASE);
    portENABLE_INTERRUPTS();
    return out;
}

static int w1_send_bit(int bit) {
    portDISABLE_INTERRUPTS();
    if (bit) {
        gpio_set_level(DS_GPIO, 0);
        ets_delay_us(DELAY_WRITE_1_START);
        gpio_set_level(DS_GPIO, 1);
        ets_delay_us(DELAY_WRITE_1_END);
    } else {
        gpio_set_level(DS_GPIO, 0);
        ets_delay_us(DELAY_WRITE_0_START);                
        gpio_set_level(DS_GPIO, 1);                        
        ets_delay_us(DELAY_WRITE_0_END); 
    }
    portENABLE_INTERRUPTS();
    return 0;
}

static int w1_reset(void) {
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(DELAY_RESET_SET);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(DELAY_RESET_WAIT);
    if (gpio_get_level(DS_GPIO) == 1) {
        return 1; 
        }
    ets_delay_us(DELAY_RESET_READ);
    return !(gpio_get_level(DS_GPIO));
}

static uint8_t w1_read_byte(void) {
    int val = 0;
    for (unsigned i = 0; i < 8; i++) {
        val = val + (w1_read_bit() << i);
    }
    return val;
}

static int w1_send_byte(uint8_t byte) {
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    for (unsigned i = 0; i < 8; i++) {
        w1_send_bit((byte & (1 << i)));
    }
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    return 0;
}

static void w1_read_scpad(w1_memory_t* d) { 
    d->t.l     = w1_read_byte();
    d->t.h     = w1_read_byte();
    d->t_tmp.l = w1_read_byte();
    d->t_tmp.h = w1_read_byte();
    d->conf    = w1_read_byte();
    d->res0    = w1_read_byte();
    d->res1    = w1_read_byte();
    d->res2    = w1_read_byte();
    d->crc     = w1_read_byte();
}

static void w1_temp_err(void) {
    DEBUG_PRINTF("Temperature probe did not respond");
}

float get_temperature(void) {
    float t_out;
    w1_memory_t d; 
    if (w1_reset()) {
        w1_temp_err();
        return 0;
        }
    w1_send_byte(W1_CMD_SKIP_ROM);                          // Skip Rom (command all boards on bus)
    w1_send_byte(W1_CMD_CONV_T);                            // Convert T (write temper to scratchpad)
    ets_delay_us(DELAY_GET_TEMP);
    if (w1_reset()) {
        w1_temp_err();
        return 0;
    }
    w1_send_byte(W1_CMD_SKIP_ROM);                          // Skip Rom
    w1_send_byte(W1_CMD_READ_SCP);                          // Read Scratchpad
    w1_read_scpad(&d);
    if (d.t.w > 0xfc00) {                                   // Negative numbers
        d.t.w = - d.t.w + 0xf920;
    }
    t_out = d.t.w / 16.0f;                                  // Conversion to degC
    DEBUG_PRINTF("T: %f C", t_out);
    return t_out;
}

int temp_init(void) {
    gpio_pad_select_gpio(DS_GPIO);
    if (w1_reset()) {
        w1_temp_err();
        return 1;
    }
    return 0;
}
