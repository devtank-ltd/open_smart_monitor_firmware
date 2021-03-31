#include "scpi.h"
#include "stdio.h"
#include "string.h"
#include "logging.h"
#define NOTQUESTIONABLE 1
#define NOTSETTABLE     2
#define SCPI_UNKNOWN    3

typedef void (*scpi_operation)(void * node);

struct scpi_node_t {
    const char * name;
    scpi_operation query_fn;
    scpi_operation setter_fn;
    struct scpi_node_t * children[];
};

#define MAXPARSEDEPTH 20
#define ERRORQUEUELEN 20
#define SCPI_PRINTF(f_, ...) printf(("\033[39m" f_ "\n"), ##__VA_ARGS__)

struct scpi_node_t * stack[MAXPARSEDEPTH] = {0};
int error_queue[ERRORQUEUELEN] = {0};
static int stackpointer = 0;

void scpi_error(int error_code) {
    static int in = 0;
    error_queue[in++] = error_code;
    in %= ERRORQUEUELEN;
    DEBUG_PRINTF("Enqueuing error code %d", error_code);
}

int scpi_stack_query(struct scpi_node_t * n) {
    for(int ptr = stackpointer; ptr >= 0; ptr--) {
        if(stack[ptr] == n) return 1;
    }
    return 0;
}

// SMART FACTORY SPECIFIC STUFF STARTS HERE
#include "volume.h"
#include "stats.h"
#include "config.h"

struct scpi_node_t frequency_node;
struct scpi_node_t pulse_node;
struct scpi_node_t pulsein1;
struct scpi_node_t pulsein2;
struct scpi_node_t hpm_pm25;
struct scpi_node_t hpm_pm10;
struct scpi_node_t temperature;
struct scpi_node_t humidity;
struct scpi_node_t sound;
struct scpi_node_t light;
struct scpi_node_t phase1;
struct scpi_node_t phase2;
struct scpi_node_t phase3;
struct scpi_node_t voltage;
struct scpi_node_t current;
struct scpi_node_t powerfactor;
struct scpi_node_t battery;

int scpi_node_to_param() {
    if(scpi_stack_query(&hpm_pm25))    return parameter_pm25;
    if(scpi_stack_query(&hpm_pm10))    return parameter_pm10;
    if(scpi_stack_query(&temperature)) return parameter_temperature;
    if(scpi_stack_query(&humidity))    return parameter_humidity;
    if(scpi_stack_query(&sound))       return parameter_sound;
    if(scpi_stack_query(&light))       return parameter_light;
    if(scpi_stack_query(&powerfactor)) return parameter_powerfactor;
    
    if(scpi_stack_query(&frequency_node) &&
       scpi_stack_query(&pulsein1))    return parameter_freq1;
    if(scpi_stack_query(&frequency_node) &&
       scpi_stack_query(&pulsein2))    return parameter_freq2;

    if(scpi_stack_query(&pulse_node) &&
       scpi_stack_query(&pulsein1))    return parameter_pulse1;
    if(scpi_stack_query(&pulse_node) &&
       scpi_stack_query(&pulsein2))    return parameter_pulse2;

    if(scpi_stack_query(&phase1) &&
       scpi_stack_query(&voltage))     return parameter_voltage1;
    if(scpi_stack_query(&phase1) &&
       scpi_stack_query(&current))     return parameter_current1;
    if(scpi_stack_query(&phase2) &&
       scpi_stack_query(&voltage))     return parameter_voltage2;
    if(scpi_stack_query(&phase2) &&
       scpi_stack_query(&current))     return parameter_current2;
    if(scpi_stack_query(&phase3) &&
       scpi_stack_query(&voltage))     return parameter_voltage3;
    if(scpi_stack_query(&phase3) &&
       scpi_stack_query(&current))     return parameter_current3;

    if(scpi_stack_query(&battery))     return parameter_battery_pc;

    DEBUG_PRINTF("Unknown parameter!");
    esp_restart();
    return 1;
}

void idn_query(void * node) {
    SCPI_PRINTF("Devtank;OSM-1");
}

void frequency_query(void * node) {
    if(scpi_stack_query(&pulsein1)) {
        SCPI_PRINTF("%dHZ", freq_get1());
        return;
    }
    if(scpi_stack_query(&pulsein2)) {
        SCPI_PRINTF("%dHZ", freq_get2());
        return;
    }
    
    scpi_error(SCPI_UNKNOWN);
}

void pulse_query(void * node) {
    if(scpi_stack_query(&pulsein1)) {
        SCPI_PRINTF("%d", pulse_get1());
        return;
    }
    if(scpi_stack_query(&pulsein2)) {
        SCPI_PRINTF("%d", pulse_get2());
        return;
    }
    
    scpi_error(SCPI_UNKNOWN);
}

void scpi_set_update_rate(void * argument) {
    int sample = atoi((char *)argument);
    int parameter = scpi_node_to_param();
    set_timedelta(parameter, sample);
    if(parameter == parameter_powerfactor)
        set_timedelta(parameter_pfleadlag, sample);
    if(parameter == parameter_battery_pc)
        set_timedelta(parameter_battery_mv, sample);
}

void scpi_get_update_rate(void * nothing) {
    SCPI_PRINTF("%u", get_timedelta(scpi_node_to_param()));
}

void scpi_set_sample_rate(void * argument) {
    int sample = atoi((char *)argument);
    int parameter = scpi_node_to_param();
    set_sample_rate(parameter, sample);
    if(parameter == parameter_powerfactor)
        set_sample_rate(parameter_pfleadlag, sample);
    if(parameter == parameter_battery_pc)
        set_sample_rate(parameter_battery_mv, sample);
}

void scpi_get_sample_rate(void * nothing) {
    SCPI_PRINTF("%u", get_sample_rate(scpi_node_to_param()));
}

struct scpi_node_t update_rate = {
    .name = "UPDaterate",
    .children = {NULL},
    .query_fn = scpi_get_update_rate,
    .setter_fn = scpi_set_update_rate,
};

struct scpi_node_t sample_rate = {
    .name = "SAMPlerate",
    .children = {NULL},
    .query_fn = scpi_get_sample_rate,
    .setter_fn = scpi_set_sample_rate,
};

struct scpi_node_t frequency_node = {
    .name = "FREQuency",
    .children = {&update_rate, &sample_rate},
    .query_fn = frequency_query,
    .setter_fn = NULL
};

struct scpi_node_t pulse_node = {
    .name = "PULSe",
    .children = {&update_rate, &sample_rate},
    .query_fn = pulse_query,
    .setter_fn = NULL,
};

#define hpm_children {&update_rate, &sample_rate, NULL}
struct scpi_node_t hpm_pm25 = {
    .name = "PM25",
    .children = hpm_children,
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

struct scpi_node_t hpm_pm10 = {
    .name = "PM10",
    .children = hpm_children,
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

struct scpi_node_t light = {
    .name = "LIGHt", 
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

struct scpi_node_t battery = {
    .name = "BATTery", 
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

#define phase_children {&voltage, &current, NULL}
struct scpi_node_t voltage = {
    .name = "VOLTage", 
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

struct scpi_node_t current = {
    .name = "CURRent", 
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};

struct scpi_node_t phase1 = { .name = "PHase1", .children = phase_children, .query_fn = NULL, .setter_fn = NULL };
struct scpi_node_t phase2 = { .name = "PHase2", .children = phase_children, .query_fn = NULL, .setter_fn = NULL };
struct scpi_node_t phase3 = { .name = "PHase3", .children = phase_children, .query_fn = NULL, .setter_fn = NULL };

struct scpi_node_t powerfactor = {
    .name = "PowerFactor",
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL, // FIXME: it would be nice if we could query this over SCPI too.
    .setter_fn = NULL
};


#define pulse_children {&pulse_node, &frequency_node, NULL}

struct scpi_node_t temperature = {
    .name = "TEMPerature",
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL,
    .setter_fn = NULL
};

struct scpi_node_t humidity = {
    .name = "HUMidity",
    .children = {&update_rate, &sample_rate, NULL},
    .query_fn = NULL,
    .setter_fn = NULL
};

struct scpi_node_t sound = {
    .name = "SOUNd",
    .children = {&update_rate, NULL},
    .query_fn = NULL,
    .setter_fn = NULL
};

struct scpi_node_t pulsein1 = {
    .name = "CHANnel1",
    .children = pulse_children,
    .query_fn = NULL,
    .setter_fn = NULL,
};

struct scpi_node_t pulsein2 = {
    .name = "CHANnel2",
    .children = pulse_children,
    .query_fn = NULL,
    .setter_fn = NULL,
};


void scpi_mqtt_query(void * argument) {
    uint8_t s = get_mqtten();
    switch(s) {
    case MQTT_DISABLE:
        SCPI_PRINTF("DISABLED");
        break;
    case MQTT_LOG_TO_USB:
        SCPI_PRINTF("USBLOG");
        break;
    case MQTTSN_OVER_LORA:
        SCPI_PRINTF("LORA");
        break;
    case MQTTSN_OVER_LORAWAN:
        SCPI_PRINTF("LORAWAN");
        break;
    default:
        scpi_error(SCPI_UNKNOWN);
        break;
    }
}

void scpi_mqtt_setter(void * argument) {
    char * arg = argument;
    DEBUG_PRINTF("argument to SCPI command \"MQTT\": -%s-", arg);
    if(strstr(arg, "DISABLED")) {
        set_mqtten(MQTT_DISABLE);
        DEBUG_PRINTF("Changed MQTT setting. Don't forget to reboot");
        return;
    }
    if(strstr(arg, "USBLOG")) {
        set_mqtten(MQTT_LOG_TO_USB);
        DEBUG_PRINTF("Changed MQTT setting. Don't forget to reboot");
        return;
    }
    if(strstr(arg, "LORA")) {
        set_mqtten(MQTTSN_OVER_LORA);
        DEBUG_PRINTF("Changed MQTT setting. Don't forget to reboot");
        return;
    }
    if(strstr(arg, "LORAWAN")) {
        set_mqtten(MQTTSN_OVER_LORAWAN);
        DEBUG_PRINTF("Changed MQTT setting. Don't forget to reboot");
        return;
    }
    scpi_error(SCPI_UNKNOWN);
}


struct scpi_node_t mqtt = {
    .name = "MQTT",
    .children = {NULL},
    .query_fn = scpi_mqtt_query,
    .setter_fn = scpi_mqtt_setter
};


// IEEE-448.1 STUFF GOES HERE
struct scpi_node_t idn = {
    .name = "*IDN",
    .children = {NULL},
    .query_fn = idn_query,
    .setter_fn = NULL,
};


// THE ROOT NODE AND CODE TO TRAVERSE THE PARSE TREE
struct scpi_node_t root = {
    .name = "ROOT",
    .children = {&pulsein1, &pulsein2, &hpm_pm25, &hpm_pm10, &idn, &mqtt,
        &temperature, &humidity, &light, &battery, &sound,
       &phase1, &phase2, &phase3, &powerfactor, NULL},
};

void scpi_parse_node(const char * string, struct scpi_node_t * node) {
    // FIXME:
    // For a given command, say "CHANnel1:FREQuency?", we need to match
    // CHAN1:FREQ?
    // CHANNEL1:FREQ?
    // chan1:frequency?
    //
    // i.e. a case-insensitive match of either all letters in the node's
    // name, or only the capital letters in the name.
    // strncmp won't cut the mustard.

    if(!strncmp(string, "?", 1)) {
        DEBUG_PRINTF("Querying %s\n", node->name);
        if(node->query_fn) node->query_fn(node);
        else scpi_error(NOTQUESTIONABLE);
        return;
    }

    if(!strncmp(string, " ", 1) || !strncmp(string, ";", 1)) {
        DEBUG_PRINTF("Setting %s\n", node->name);
        if(node->setter_fn) node->setter_fn((void *) string);
        else scpi_error(NOTSETTABLE);
        return;
    }

    if(!strncmp(string, ":", 1))
        string++;

    for(int i = 0; node->children[i]; i++) {
        if(!strncmp(node->children[i]->name, string, strlen(node->children[i]->name))) {
            stack[stackpointer++] = node->children[i];
            scpi_parse_node(string + strlen(node->children[i]->name), node->children[i]);
            return;
        }
    }

    scpi_error(SCPI_UNKNOWN);
}

void scpi_parse(char * string) {
    stackpointer = 0;
    scpi_parse_node(string, &root);
}
