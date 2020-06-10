#pragma once
void heartbeat();

void mqtt_announce_int(const char * key, int val);
void mqtt_announce_str(char * key, char * val);
