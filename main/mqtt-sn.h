#pragma once
int mqtt_announce_dropped();
int heartbeat();

int mqtt_announce_int(const char * key, int val);
int mqtt_announce_str(const char * key, const char * val);
int mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta);

