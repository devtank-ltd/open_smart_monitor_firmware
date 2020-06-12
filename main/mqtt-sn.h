#pragma once
void mqtt_announce_dropped();
void heartbeat();

void mqtt_announce_int(const char * key, int val);
void mqtt_announce_str(char * key, char * val);
void mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta);

