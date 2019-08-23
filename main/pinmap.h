/* 
 * The Honeywell Particulate Matter sensor will be connected to UART1. But the
 * default pins for UART2 are 9 and 10, but these are also used by the flash
 * memory. So to stick with the defaults will give rise to a IllegalInstruction
 * exception (or LoadProhibited or anything)
 */
#define HPM_UART    UART_NUM_1
#define HPM_UART_RX 2
#define HPM_UART_TX 4

/*
 * The LoRa module is connected to UART2.
 * For this UART, the default pins seem to work okay.
 */
#define LORA_UART    UART_NUM_2
#define LORA_UART_RX 16
#define LORA_UART_TX 17

