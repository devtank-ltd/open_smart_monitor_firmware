#define BUFLEN 256
#define KEYLEN 16
#define VALLEN 50

extern char value[VALLEN];
void configure();
void store_config(char * key, char * val);
void get_config(const char * key);
