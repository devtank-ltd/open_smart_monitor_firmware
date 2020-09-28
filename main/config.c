#include "pinmap.h"
#include "driver/uart.h"
#include "mac.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"
#include "logging.h"
#include "mac.h"

static char value[VALLEN];


static inline int is_whitespace(uint8_t c) {
    return c == '\t' || c == ' ' || c == '\n';
}

static inline char * getfield(char * f) {
    while(!is_whitespace(f[0])) f++;
    f[0] = '\0';
    f++;
    while(is_whitespace(f[0])) f++;
    return f;
}

void store_config(char * key, char * val) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ERROR_PRINTF("Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    nvs_set_str(my_handle, key, val);
    nvs_commit(my_handle);
}

nvs_handle_t calibration_handle() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ERROR_PRINTF("Error (%s) opening NVS handle!", esp_err_to_name(err));
        return (nvs_handle_t)NULL;
    }
    return my_handle;
}

const char* get_config(const char * key) {
    size_t len = VALLEN;
    nvs_get_str(calibration_handle(), key, value, &len);
    INFO_PRINTF("%s %s %s", mac_addr, key, value);
    return value;
}

void set_midpoint(float v) {
    esp_err_t err = nvs_set_u32(calibration_handle(), "midpoint", v * 10000);
    if (err != ESP_OK) {
        ERROR_PRINTF("Error (%s) setting midpoint!", esp_err_to_name(err));
    }
}

float get_midpoint() {
    unsigned int millivolts = 0;
    esp_err_t err = nvs_get_u32(calibration_handle(), "midpoint", &millivolts);
    if(err != ESP_OK) {
        if(!strcmp(mac_addr, "98f4ab14737d")) set_midpoint(1.630500);
        if(!strcmp(mac_addr, "98f4ab147445")) set_midpoint(1.635000);
        if(!strcmp(mac_addr, "98f4ab147395")) set_midpoint(1.656300);
        if(!strcmp(mac_addr, "98f4ab147449")) set_midpoint(1.637800);
        if(!strcmp(mac_addr, "98f4ab1474ad")) set_midpoint(1.680100);
        if(!strcmp(mac_addr, "98f4ab147441")) set_midpoint(1.690000);
        if(!strcmp(mac_addr, "98f4ab147439")) set_midpoint(1.653900);
        set_midpoint(1.6804);
        return get_midpoint();
    } else {
       return millivolts / 10000;
    }
}
