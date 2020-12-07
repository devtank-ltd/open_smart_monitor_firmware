#pragma once

#define STATUS_LED_OK       0
#define STATUS_LED_TROUBLE  1

void status_led_set_status(int status);
void status_led_task(void *pvParameters);
