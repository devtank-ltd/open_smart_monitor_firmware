#include <unistd.h>

#include "esp_system.h"
#include "driver/gpio.h"
#include "pinmap.h"

typedef union {
    struct {
        uint16_t W;
    } full;
    struct {
        uint8_t L, H;
    } half;
} t_data;

struct data {
    t_data t;
    t_data t_tmp;
    uint8_t conf;
    uint8_t res0;
    uint8_t res1;
    uint8_t res2;
    uint8_t crc;
};

int read_bit(void) {
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OW_GPIO, 1);
    usleep(2);
    gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
    usleep(15);
    return gpio_get_level(OW_GPIO);
}

int send_bit(int bit) {
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OW_GPIO, 0);
    usleep(5);
    gpio_set_level(OW_GPIO, bit);
    usleep(55);
    return 0;
}

int reset(void) {
    gpio_set_direction(OW_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OW_GPIO, 0);
    usleep(480);
    gpio_set_direction(OW_GPIO, GPIO_MODE_INPUT);
    usleep(60);
    if (gpio_get_level(OW_GPIO) == 1) {
        return 1; }
    usleep(240);
    if (gpio_get_level(OW_GPIO) == 0) {
        return 1; }
    else {
        return 0; }
}

int read_byte(void) {
    int val = 0;
    for (int i = 0; i < 8; ++i) {
        val = val + read_bit();
    }
    return val;
}

void read_scpad(struct data* d) {
    d->t.half.H     = read_byte();
    d->t.half.L     = read_byte();
    d->t_tmp.half.H = read_byte();
    d->t_tmp.half.L = read_byte();
    d->conf         = read_byte();
    d->res0         = read_byte();
    d->res1         = read_byte();
    d->res2         = read_byte();
    d->crc          = read_byte();
}

int send_hex(uint8_t hex) {
    for (uint8_t i = 0x0; i < 0xF; ++i) {
        if ((hex & i) == 0) {
            send_bit(0); }
        else {
            send_bit(1); }
    }
    return 0;
}

int get_temp(void) {
    struct data d; 
    reset();
    send_hex(0xCC); // Skip Rom (command all boards on bus)
    send_hex(0x44); // Convert T (write temperature to scratchpad)
    send_hex(0xBE); // Read Scratchpad
    read_scpad(&d); // Save data to this structure
    return d.t.full.W;
}

int main(void) {
    int temp = get_temp();
    printf("%u", temp);
    return 0;
}
