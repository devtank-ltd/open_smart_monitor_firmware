#include "pinmap.h"
#include "driver/uart.h"
#include "mac.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"
#include "logging.h"

static char value[VALLEN];


static inline int is_whitespace(uint8_t c) {
    return c == '\t' || c == ' ' || c == '\n';
}

static inline int copy_along(uint8_t * dst, uint8_t * src, int len) {
    size_t ret = 0;
    while(len && !is_whitespace(*src)) {
        *dst++ = *src++;
        len--;
        ret++;
    }
    return ret;
}

static inline int separate(uint8_t * str, int cur) {
    while(is_whitespace(str[cur])) cur++;
    return cur;
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

const char* get_config(const char * key) {
    nvs_handle_t my_handle;
    size_t len = VALLEN;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ERROR_PRINTF("Error (%s) opening NVS handle!", esp_err_to_name(err));
        value[0] = '\0';
        return value;
    }
    nvs_get_str(my_handle, key, value, &len);
    INFO_PRINTF("%s %s %s", mac_addr, key, value);
    return value;
}

void configure() {
    char message[BUFLEN];
    size_t length = 0;
    char * mac = NULL;
    char * key = NULL;
    char * val = NULL;
    uart_get_buffered_data_len(LORA_UART, &length);
    if(!length) return;
    length = 0;
keep_listening:
    length += uart_read_bytes(LORA_UART, (uint8_t *)message + length, BUFLEN - length, 1);
    message[length] = '\0';
    if(length > BUFLEN)             goto procline; // buffer overrun
    if(message[length - 1] == '\r') goto procline; // carriage return
    if(message[length - 1] == '\b') length -= 2;   // backspace

    vTaskDelay(1);
    goto keep_listening;

procline:
    mac = message;
    key = getfield(mac);
    val = getfield(key);
    getfield(val);

    if(strcmp(mac, mac_addr)) return;

    INFO_PRINTF("mac %zu %s", strlen(mac), mac);
    INFO_PRINTF("key %zu %s", strlen(key), key);
    INFO_PRINTF("val %zu %s", strlen(val), val);
    store_config(key, val);
    return;
}