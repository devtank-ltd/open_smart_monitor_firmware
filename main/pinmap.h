/* 
 * The Honeywell Particulate Matter sensor will be connected to UART1. But the
 * default pins for UART2 are 9 and 10, but these are also used by the flash
 * memory. So to stick with the defaults will give rise to a IllegalInstruction
 * exception (or LoadProhibited, or the device will not flash, or ...)
 */
#define HPM_UART    UART_NUM_1
#define HPM_UART_RX 2
#define HPM_UART_TX 4

/*
 * The TSL2591 ambient light sensor, which is connected to the I2C bus as a
 * slave at address 0x29.
 */
#define TLS2591_ADDR 0x29

/* 
 * Pin assignments for the I2C bus.
 */
#define I2CPIN_MASTER_SDA 0x18
#define I2CPIN_MASTER_SCL 0x19
#define I2CPIN_SLAVE_SDA  0x25
#define I2CPIN_SLAVE_SCL  0x26

/*
 * The LoRa module is connected to UART2.
 * For this UART, the default pins seem to work okay.
 */
#define LORA_UART    UART_NUM_2
#define LORA_UART_RX 16
#define LORA_UART_TX 17

