#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
int mqtt_announce_dropped();
int heartbeat();

int mqtt_announce_int(const char * key, int val);
int mqtt_announce_str(const char * key, const char * val);
int mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta);

typedef struct _ {
    TickType_t updated;
    TickType_t sent;
    TickType_t delta;
    int32_t value;
} mqtt_datum_t;

typedef struct __ {
    TickType_t updated;
    TickType_t sent;
    TickType_t delta;
    int64_t average;
    int32_t minimum;
    int32_t maximum;
} mqtt_stats_t;

extern mqtt_stats_t humidity_stats;
extern mqtt_stats_t sound_stats;
extern mqtt_stats_t temperature_stats;
extern mqtt_datum_t battery_mv_datum;
extern mqtt_datum_t battery_pc_datum;
extern mqtt_stats_t pm10_stats;
extern mqtt_stats_t pm25_stats;
extern mqtt_stats_t visible_light_stats;
extern mqtt_stats_t current1_stats;
extern mqtt_stats_t current2_stats;
extern mqtt_stats_t current3_stats;
extern mqtt_stats_t voltage1_stats;
extern mqtt_stats_t voltage2_stats;
extern mqtt_stats_t voltage3_stats;
extern mqtt_datum_t water_meter_datum;
extern mqtt_datum_t gas_meter_datum;
extern mqtt_datum_t export_energy_datum;
extern mqtt_datum_t import_energy_datum;
extern mqtt_stats_t pf_stats;
extern mqtt_stats_t pf_sign_stats;
void mqtt_task(void *pvParameters);
void mqtt_datum_update(mqtt_datum_t * datum, int32_t value);
void mqtt_datum_update_delta(mqtt_datum_t * datum, int32_t mins);
