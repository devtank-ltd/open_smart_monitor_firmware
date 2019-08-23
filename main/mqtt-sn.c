#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pinmap.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"

#define MQTT_SN_MAX_PACKET_LENGTH     (255)
#define MQTT_SN_TYPE_PUBLISH          (0x0C)
#define MQTT_SN_FLAG_QOS_N1           (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN           (0x1 << 4)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)

// This funcion was shamelessly stolen from https://github.com/njh/DangerMinusOne/blob/master/DangerMinusOne.ino
// and adapted to Actual C by me.
void mqtt_sn_send(const char topic[2], char * message, bool retain)
{
  char header[7];

  header[0] = sizeof(header) + strlen(message);
  header[1] = MQTT_SN_TYPE_PUBLISH;
  header[2] = MQTT_SN_FLAG_QOS_N1 | MQTT_SN_TOPIC_TYPE_SHORT;
  if (retain) header[2] |= MQTT_SN_FLAG_RETAIN;

  header[3] = topic[0];
  header[4] = topic[1];
  header[5] = 0x00;  // Message ID High
  header[6] = 0x00;  // message ID Low
  

  uart_tx_chars(LORA_UART, header, 7);
  uart_tx_chars(LORA_UART, message, strlen(message));
}

void mqtt_sn_connect() {

}
