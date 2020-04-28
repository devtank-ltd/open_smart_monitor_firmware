#pragma once
#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 50

void configure();
void store_config(char * key, char * val);
const char* get_config(const char * key);
