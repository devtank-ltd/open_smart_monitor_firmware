#pragma once
void smart_meter_setup();
void smart_meter_query();
void smart_meter_announce();
uint16_t modbus_crc(const uint8_t * buf, unsigned length);
