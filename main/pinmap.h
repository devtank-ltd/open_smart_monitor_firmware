#pragma once
/*
 * There's a status LED on GPIO33.
 */
#define STATUS_LED 33

/*
 * The Honeywell Particulate Matter sensor will be connected to UART1. But the
 * default pins for UART2 are 9 and 10, but these are also used by the flash
 * memory. So to stick with the defaults will give rise to a IllegalInstruction
 * exception (or LoadProhibited, or the device will not flash, or ...)
 */
#define DEVS_UART    UART_NUM_1
#define DEVS_UART_RX 17
#define DEVS_UART_TX 16

/*
 * The TSL2591 ambient light sensor, which is connected to the I2C bus as a
 * slave at address 0x29.
 */
#define TLS2591_ADDR 0x29

/*
 * Pin assignments for the I2C bus.
 */
#define I2CPIN_MASTER_SDA 21
#define I2CPIN_MASTER_SCL 22

/*
 * Pin assignments for the sound detector
 */

#define SOUND_ENVELOPE  ADC1_CHANNEL_3
#define SOUND_OUTPUT    ADC1_CHANNEL_6
#define SOUND_GATE      36

/*
 * Pin assignments for health monitoring
 */
 #define BATMON        ADC2_CHANNEL_4

/*
 * UART mux.
 * We ran out of UARTs so we need to multiplex. This pin is brought high or low to choose the port
 */
#define SW_SEL 27
#define SW_EN 13
#define RS485_DE 25
/*
 * The LoRa module is connected to UART2. We cannot use pins 9 and 10 on the
 * prototype, because these are involved in SPI bus that connects the flash
 * memory to the ESP32. So we'll just use the same pins as on the olimex board.
 */
#define LORA_UART     UART_NUM_2
#define LORA_UART_RX  19
#define LORA_UART_TX  18
#define LORA_UART_CTS 23

#define PULSE_IN_1  32
#define PULSE_IN_2  35
#define DS_GPIO     14
#define LIGHT_INT   4
#define TEMP_INT    26
#define I2CBUS I2C_NUM_0

#define BAT_MON ADC2_CHANNEL_4
#define POWER_INT 14

#define UPLINK_UART  UART_NUM_0
