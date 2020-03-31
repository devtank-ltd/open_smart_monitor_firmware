#include "esp_wifi.h"
#include "mac.h"
#include "nvs_flash.h"
#include "logging.h"

uint8_t mac[6];
char mac_addr[13];

void getmac() {
    nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err;

    err = esp_wifi_init(&cfg);
    char * error = esp_err_to_name(err);
    ERROR_PRINTF(error);
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA, mac));
    snprintf(mac_addr, 13, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    INFO_PRINTF("My MAC address is %s.", mac_addr);
    ESP_ERROR_CHECK(esp_wifi_deinit());
}
