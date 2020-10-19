#pragma once
void smart_meter_setup();
void smart_meter_query();
uint16_t modbus_crc(const uint8_t * buf, unsigned length);
