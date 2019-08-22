/* 
 * The Honeywell Particulate Matter sensor will be connected to UART1. But the
 * default pins for UART2 are 9 and 10, but these are also used by the flash
 * memory. So to stick with the defaults will give rise to a IllegalInstruction
 * exception (or LoadProhibited or anything)
 */
#define HPMUART    UART_NUM_1
#define HPMUART_RX 2
#define HPMUART_TX 4

/*
 * The LoRa module is connected to UART2.
 * For this UART, the default pins seem to work okay.
 */
#define LORAUART    UART_NUM_2
#define LORAUART_RX 16
#define LORAUART_TX 17

