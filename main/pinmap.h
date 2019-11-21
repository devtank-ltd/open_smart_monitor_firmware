/* 
 * The Honeywell Particulate Matter sensor will be connected to UART1. But the
 * default pins for UART2 are 9 and 10, but these are also used by the flash
 * memory. So to stick with the defaults will give rise to a IllegalInstruction
 * exception (or LoadProhibited, or the device will not flash, or ...)
 */
#define HPM_UART    UART_NUM_1
#define HPM_UART_RX 16
#define HPM_UART_TX 17

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

#define SOUND_ENVELOPE 33
#define SOUND_GATE     25

/*
 * UART mux.
 * We ran out of UARTs so we need to multiplex. This pin is brought high or low to choose the port
 */
#define UART_MUX 27

/*
 * The LoRa module is connected to UART2. We cannot use pins 9 and 10 on the 
 * prototype, because these are involved in SPI bus that connects the flash
 * memory to the ESP32. So we'll just use the same pins as on the olimex board.
 */
#define LORA_UART     UART_NUM_2
#define LORA_UART_RX  19
#define LORA_UART_TX  18
#define LORA_UART_CTS 23

#define PULSE_IN_1  12
#define PULSE_IN_2  5

#define I2CBUS I2C_NUM_0
