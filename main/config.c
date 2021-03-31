#include "pinmap.h"
#include "driver/uart.h"
#include "mac.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"
#include "logging.h"
#include "stats.h"
#include "mac.h"

nvs_handle_t cal_handle;
nvs_handle_t delta_handle;
nvs_handle_t samplerate_handle;

void config_init() {
    nvs_flash_init();
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &cal_handle));
    ESP_ERROR_CHECK(nvs_open("delta", NVS_READWRITE, &delta_handle));
    ESP_ERROR_CHECK(nvs_open("samplerate", NVS_READWRITE, &samplerate_handle));
}

void set_midpoint(float v) {
    esp_err_t err = nvs_set_u32(cal_handle, "midpoint", v * 10000);
    ERROR_PRINTF("(%s) setting midpoint!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

float get_midpoint() {
    unsigned int millivolts = 0;
    esp_err_t err = nvs_get_u32(cal_handle, "midpoint", &millivolts);
    if(err != ESP_OK) {
             if(!strcmp(mac_addr, "98f4ab14737d")) set_midpoint(1.630500);
        else if(!strcmp(mac_addr, "98f4ab147445")) set_midpoint(1.635000);
        else if(!strcmp(mac_addr, "98f4ab147395")) set_midpoint(1.656300);
        else if(!strcmp(mac_addr, "98f4ab147449")) set_midpoint(1.637800);
        else if(!strcmp(mac_addr, "98f4ab1474ad")) set_midpoint(1.680100);
        else if(!strcmp(mac_addr, "98f4ab147441")) set_midpoint(1.690000);
        else if(!strcmp(mac_addr, "98f4ab147439")) set_midpoint(1.653900);
        else set_midpoint(1.6804);
        return get_midpoint();
    } else {
       return millivolts / 10000.0;
    }
}

void set_pulsein1(uint8_t en) {
    esp_err_t err = nvs_set_u8(cal_handle, "pulsein1", en);
    ERROR_PRINTF("(%s) setting pulsein1!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint8_t get_pulsein1() {
    uint8_t en = 0;
    esp_err_t err = nvs_get_u8(cal_handle, "pulsein1", &en);
    ERROR_PRINTF("(%s) getting pulsein1!", esp_err_to_name(err));
    if(err != ESP_OK) {
        set_pulsein1(PULSEIN_UNUSED);
        return get_pulsein1();
    } else {
       return en;
    }
}

void set_pulsein2(uint8_t en) {
    esp_err_t err = nvs_set_u8(cal_handle, "pulsein2", en);
    ERROR_PRINTF("(%s) setting pulsein2!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint8_t get_pulsein2() {
    uint8_t en = 0;
    esp_err_t err = nvs_get_u8(cal_handle, "pulsein2", &en);
    ERROR_PRINTF("(%s) getting pulsein2!", esp_err_to_name(err));
    if(err != ESP_OK) {
        set_pulsein2(PULSEIN_UNUSED);
        return get_pulsein2();
    } else {
       return en;
    }
}

void set_wateroffset(uint32_t offs) {
    esp_err_t err = nvs_set_u32(cal_handle, "water_offs", offs);
    ERROR_PRINTF("(%s) setting water_offs!", esp_err_to_name(err));
}


void set_hpmen(uint8_t en) {
    // Can this function  be nixxed?
    esp_err_t err = nvs_set_u8(cal_handle, "hpm_en", en);
    ERROR_PRINTF("(%s) setting hpm_en!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint8_t get_hpmen() {
    // Can this function  be nixxed?
    uint8_t en = 0;
    esp_err_t err = nvs_get_u8(cal_handle, "hpm_en", &en);
    ERROR_PRINTF("(%s) getting hpm_en; it's %d.!", esp_err_to_name(err), (int)en);
    if(err != ESP_OK) {
        set_hpmen(0);
        return get_hpmen();
    } else {
        return en;
    }
}

void set_socoen(uint8_t en) {
    // Can this function  be nixxed?
    esp_err_t err = nvs_set_u8(cal_handle, "soco_en", en);
    if(err !=ESP_OK)
        ERROR_PRINTF("(%s) setting soco_en!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint8_t get_socoen() {
    // Can this function  be nixxed?
    uint8_t en = 0;
    esp_err_t err = nvs_get_u8(cal_handle, "soco_en", &en);
    if(err !=ESP_OK)
        ERROR_PRINTF("(%s) getting soco_en!", esp_err_to_name(err));
    if(err != ESP_OK) {
        set_socoen(!get_hpmen());
        return get_socoen();
    } else {
        return en;
    }
}

void set_mqtten(uint8_t en) {
    esp_err_t err = nvs_set_u8(cal_handle, "mqtt_en", en);
    if(err !=ESP_OK)
        ERROR_PRINTF("(%s) setting mqtt_en!", esp_err_to_name(err));
    err = nvs_commit(cal_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
    get_mqtten();
}

uint8_t get_mqtten() {
    uint8_t en = 0;
    esp_err_t err = nvs_get_u8(cal_handle, "mqtt_en", &en);
    ERROR_PRINTF("(%s) getting mqtt_en, it's %d!", esp_err_to_name(err), (int)en);
    if(err != ESP_OK) {
        return MQTTSN_OVER_LORA;
    } else {
       return en;
    }
}

void set_timedelta(int parameter, uint32_t delta) {
    const char * parameter_name = parameter_names[parameter];
    esp_err_t err = nvs_set_u32(delta_handle, parameter_name, delta);
    if(err !=ESP_OK)
        ERROR_PRINTF("(%s) setting timedelta to %u for %s!", esp_err_to_name(err), delta, parameter_name);
    err = nvs_commit(delta_handle);
    ERROR_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint32_t get_timedelta(int parameter) {
    const char * parameter_name = parameter_names[parameter];
    uint32_t delta = 0;
    esp_err_t err = nvs_get_u32(delta_handle, parameter_name, &delta);
    if(err != ESP_OK) {
        ERROR_PRINTF("(%s) getting timedelta for %s!", esp_err_to_name(err), parameter_name);
        set_timedelta(parameter, 5);
        return 5;
    } else {
        return delta;
    }
}

void set_sample_rate(int parameter, uint32_t delta) {
    const char * parameter_name = parameter_names[parameter];
    esp_err_t err = nvs_set_u32(samplerate_handle, parameter_name, delta);
    if(err != ESP_OK)
        ERROR_PRINTF("(%s) setting sample rate to %u for %s!", esp_err_to_name(err), delta, parameter_name);
    err = nvs_commit(samplerate_handle);
    DEBUG_PRINTF("(%s) commiting handle", esp_err_to_name(err));
}

uint32_t get_sample_rate(int parameter) {
    const char * parameter_name = parameter_names[parameter];
    uint32_t rate = 0;
    esp_err_t err = nvs_get_u32(samplerate_handle, parameter_name, &rate);
    if(err != ESP_OK) {
        ERROR_PRINTF("(%s) getting sample rate for %s!", esp_err_to_name(err), parameter_name);
        set_sample_rate(parameter, 1);
        return 1;
    } else {
        return rate;
    }
}
