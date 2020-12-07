#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
int mqtt_announce_dropped();
int heartbeat();

int mqtt_announce_int(const char * key, int val);
int mqtt_announce_str(const char * key, const char * val);
int mqtt_delta_announce_int(const char * key, uint16_t * val, uint16_t * old, int delta);

typedef volatile struct _ {
    bool ready;
    TickType_t updated;
    TickType_t sent;
    TickType_t delta;
    int32_t value;
} mqtt_datum_t;

typedef volatile struct __ {
    bool ready;
    TickType_t updated;
    TickType_t sent;
    TickType_t delta;
    int64_t average;
    int32_t minimum;
    int32_t maximum;
} mqtt_stats_t;

extern mqtt_stats_t mqtt_humidity_stats;
extern mqtt_stats_t mqtt_sound_stats;
extern mqtt_stats_t mqtt_temperature_stats;
extern mqtt_datum_t mqtt_battery_mv_datum;
extern mqtt_datum_t mqtt_battery_pc_datum;
extern mqtt_stats_t mqtt_pm10_stats;
extern mqtt_stats_t mqtt_pm25_stats;
extern mqtt_stats_t mqtt_visible_light_stats;
extern mqtt_stats_t mqtt_current1_stats;
extern mqtt_stats_t mqtt_current2_stats;
extern mqtt_stats_t mqtt_current3_stats;
extern mqtt_stats_t mqtt_voltage1_stats;
extern mqtt_stats_t mqtt_voltage2_stats;
extern mqtt_stats_t mqtt_voltage3_stats;
extern mqtt_datum_t mqtt_water_meter_datum;
extern mqtt_datum_t mqtt_gas_meter_datum;
extern mqtt_datum_t mqtt_export_energy_datum;
extern mqtt_datum_t mqtt_import_energy_datum;
extern mqtt_stats_t mqtt_pf_stats;
extern mqtt_stats_t mqtt_pf_sign_stats;
void mqtt_task(void *pvParameters);
void mqtt_datum_update(mqtt_datum_t * datum, int32_t value);
void mqtt_datum_update_delta(mqtt_datum_t * datum, int32_t mins);
void mqtt_stats_update_delta(mqtt_stats_t * stats, int32_t mins);
