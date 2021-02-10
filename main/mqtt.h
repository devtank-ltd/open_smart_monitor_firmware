typedef struct {
    char topic[TOPICLEN];
    char suffix[SUFFIXLEN];
    char payload[PAYLOADLEN];
    char mac[MACLEN];
    TickType_t timestamp;
} msg_t;

void mqtt_enqueue_int(const char * parameter, const char * suffix, int val);
