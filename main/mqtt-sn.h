void heartbeat();

void update_pm25(uint16_t val);
void update_pm10(uint16_t val);
void update_ch0(uint16_t val);
void update_ch1(uint16_t val);
void update_hum(float val);
void update_temp(float val);
void update_volt(int mV);
void update_curr(int mA);

void mqtt_announce_int(const char * key, int val);
