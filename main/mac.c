#include "esp_wifi.h"
#include "mac.h"
#include "nvs_flash.h"
uint8_t mac[6];
char mac_addr[13];

void getmac() {
    nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err;

    err = esp_wifi_init(&cfg);
    printf("%s\n", esp_err_to_name(err));
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA, mac));
    snprintf(mac_addr, 13, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("My MAC address is %s.\n", mac_addr);
    ESP_ERROR_CHECK(esp_wifi_deinit());
}
