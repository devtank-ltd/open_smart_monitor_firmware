#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mac.h"

#define SUFFIXLEN 4
#define TOPICLEN 20
#define PAYLOADLEN 20

extern xQueueHandle mqtt_queue;

typedef struct {
    char topic[TOPICLEN];
    char suffix[SUFFIXLEN];
    char payload[PAYLOADLEN];
    char mac[MACLEN];
    TickType_t timestamp;
} msg_t;

void mqtt_enqueue_int(const char * parameter, const char * suffix, int val);
void mqtt_init();
