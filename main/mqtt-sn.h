void mqtt_sn_send(const char topic[2], const char * message, bool retain);
void mqtt_update(const char ident, const char * msg);

void update_pm25(uint16_t val);
void update_pm10(uint16_t val);
void update_ch0(uint16_t val);
void update_ch1(uint16_t val);
void update_hum(float val);
void update_temp(float val);
