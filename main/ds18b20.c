/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "pinmap.h"
#include "stats.h"

int init=0;
/// Sends one bit to bus
void ds18b20_send(char bit){
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO,0);
    ets_delay_us(5);
    if(bit==1)
        gpio_set_level(DS_GPIO,1);
    ets_delay_us(80);
    gpio_set_level(DS_GPIO,1);
}

// Reads one bit from bus
bool ds18b20_read(void){
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO,0);
    ets_delay_us(2);
    gpio_set_level(DS_GPIO,1);
    ets_delay_us(15);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    if(gpio_get_level(DS_GPIO)==1)
        return 1;
    else
        return 0;
}

// Sends one byte to bus
void ds18b20_send_byte(char data){
    unsigned char i;
    unsigned char x;
    for(i=0;i<8;i++){
        x = data>>i;
        x &= 0x01;
        ds18b20_send(x);
    }
    ets_delay_us(100);
}

// Reads one byte from bus
unsigned char ds18b20_read_byte(void){
    unsigned char i;
    unsigned char data = 0;
    for (i=0;i<8;i++)
    {
        if(ds18b20_read()) data|=0x01<<i;
        ets_delay_us(15);
    }
    return(data);
}

// Sends reset pulse
bool ds18b20_RST_PULSE(void) {
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(500);
    gpio_set_level(DS_GPIO, 1);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(30);
    gpio_get_level(DS_GPIO);
    ets_delay_us(470);

    if(gpio_get_level(DS_GPIO)==1)
        return 1;
    else
        return 0;

}

// Gets temperature from probe and enqueues the sample
void get_temp_probe(void) {
    if(init == 1) {
        unsigned char check;
        char temp1 = 0, temp2 = 0;
        check = ds18b20_RST_PULSE();
        if(check == 1) {
            ds18b20_send_byte(0xCC);
            ds18b20_send_byte(0x44);
            vTaskDelay(750 / portTICK_RATE_MS);
            check = ds18b20_RST_PULSE();
            ds18b20_send_byte(0xCC);
            ds18b20_send_byte(0xBE);
            temp1 = ds18b20_read_byte();
            temp2 = ds18b20_read_byte();
            check = ds18b20_RST_PULSE();
            float temp = ((float)(temp1+(temp2*256))/16) * 10;

            // A temperature of 40959 is what we get if it's not plugged in
            int i = temp;
            if(i != 40959)
                stats_enqueue_sample(parameter_temp_probe, i);
        }
    }
}

void ds18b20_init() {
    gpio_pad_select_gpio(DS_GPIO);
    init=1;
}
