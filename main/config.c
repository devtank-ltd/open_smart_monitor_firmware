#include "pinmap.h"
#include "driver/uart.h"
#include "mac.h"
#include <string.h>

#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 16

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

void configure() {
    char message[BUFLEN];
    size_t length = 0;
    uart_get_buffered_data_len(LORA_UART, &length);
    if(!length) return 0;
    length = 0;
keep_listening:
    length += uart_read_bytes(LORA_UART, (uint8_t *)message + length, BUFLEN - length, 1);
    message[length] = '\0';
    if(length > BUFLEN)             goto procline; // buffer overrun
    if(message[length - 1] == 0x0d) goto procline; // carriage return
    if(message[length - 1] == 0x7f) length -= 2;   // backspace
    
    vTaskDelay(1);
    goto keep_listening;

    char * mac;
    char * key;
    char * val;
    
procline:
    mac = message;
    key = getfield(mac);
    val = getfield(key);
    getfield(val);

    if(strcmp(mac, mac_addr)) return;

    printf("mac %d %s\n", strlen(mac), mac);
    printf("key %d %s\n", strlen(key), key);
    printf("val %d %s\n", strlen(val), val);
    // b4e62d94189e key val
    // b4e62d94189e      key val
    // b4e62d94189e key   val

    return 1;    
    
}
