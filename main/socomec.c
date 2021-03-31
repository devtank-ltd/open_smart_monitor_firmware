#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/uart.h"
#include "mqtt.h"
#include "driver/gpio.h"
#include "pinmap.h"
#include "logging.h"
#include "stats.h"
#include "config.h"

#define READ_HOLDING_FUNC 3
#define MODBUS_ERROR_MASK 0x80
#define NUM_SAMPLES 1000

#define E53_ADDR 5

/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                     <             PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)
*/
#define MIN_MODBUS_PACKET_SIZE    4 /*addr, func, crc*/
#define MAX_MODBUS_PACKET_SIZE    127


#include "esp_log.h"
#include <string.h> // needed for memset down there

#define TAG "MODBUS"
#define SENSE_MB_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return (ret_val); \
    }
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }
#define NOOPTS { .opt1 = 0, .opt2 = 0, .opt3 = 0 }

int sococonnected = 0;

typedef enum {
    PARAM_TYPE_U8 = 0x00,                   /*!< Unsigned 8 */
    PARAM_TYPE_U16 = 0x01,                  /*!< Unsigned 16 */
    PARAM_TYPE_U32 = 0x02,                  /*!< Unsigned 32 */
    PARAM_TYPE_FLOAT = 0x03,                /*!< Float type */
    PARAM_TYPE_ASCII = 0x04                 /*!< ASCII type */
} param_descr_type_t;

typedef struct {
    uint8_t             cid;                /*!< Characteristic cid */
    const char*         param_key;          /*!< The key (name) of the parameter */
    const char*         param_units;        /*!< The physical units of the parameter */
    uint16_t            mb_reg_start;       /*!< This is the Modbus register address. This is the 0 based value. */
    param_descr_type_t  param_type;         /*!< Float, U8, U16, U32, ASCII, etc. */
    uint8_t             param_size;         /*!< Number of bytes in the parameter. */
} dev_parameter_descriptor_t;


// The daft thing here, which is mandated by the freemodbus library, is that the Cid must be a number starting from zero and incremented by one each time. Essentially,
// each element needs a Cid equal to the subscript that identifies it, which makes adding ro removing parameters very tricky. Therefore, I recomment you only add them
// to the end of the list. Even though it means that the order of items ends up making no sense.
const dev_parameter_descriptor_t countis_e53[] = { \
 // { Cid, Param Name,                     Units,                          , Start, Data Type,      DataSize,}
    { 0,   (const char *)"Hour meter",     (const char *)"watt/hours /100" , 50512, PARAM_TYPE_U32,   4},
    { 1,   (const char *)"Apparent power", (const char *)"VA / 0.1"        , 50536, PARAM_TYPE_U32,   4},
    { 2,   (const char *)"Hour meter",     (const char *)"hours/100"       , 50592, PARAM_TYPE_U32,   4},
    { 3,   (const char *)"Network type",   (const char *)"Network type"    , 40448, PARAM_TYPE_U8,    1},
    { 4,   (const char *)"Ident",          (const char *)"Should read SOCO", 50000, PARAM_TYPE_ASCII, 8},
    { 5,   (const char *)"Vendor name",    (const char *)""                , 50042, PARAM_TYPE_ASCII, 8},
    { 6,   (const char *)"ProductOrderID", (const char *)""                , 50004, PARAM_TYPE_U16,   8},
    { 7,   (const char *)"ProductID",      (const char *)""                , 50005, PARAM_TYPE_U16,   8},
    { 8,   (const char *)"Voltage",        (const char *)"V"               , 50520, PARAM_TYPE_U32,   4},
    { 9,   (const char *)"Frequency",      (const char *)"Hz"              , 50606, PARAM_TYPE_FLOAT, 4},
    { 10,  (const char *)"Current",        (const char *)"A"               , 50528, PARAM_TYPE_U32,   4},
    { 11,  (const char *)"ActivePower",    (const char *)"P"               , 50536, PARAM_TYPE_U32,   4},
    { 12,  (const char *)"ReactivePower",  (const char *)"Q"               , 50538, PARAM_TYPE_U32,   4},
    { 13,  (const char *)"PowerFactor",    (const char *)"PF"              , 50542, PARAM_TYPE_U32,   4},
    { 14,  (const char *)"ActivePowerP1",  (const char *)"P"               , 50544, PARAM_TYPE_U32,   4},
    { 15,  (const char *)"ReactivePowerP1",(const char *)"Q"               , 50550, PARAM_TYPE_U32,   4},
    { 16,  (const char *)"PowerFactorP1",  (const char *)"PF"              , 50542, PARAM_TYPE_U32,   4},
    { 17,  (const char *)"ActivePowerP2",  (const char *)"P"               , 50546, PARAM_TYPE_U32,   4},
    { 18,  (const char *)"ReactivePowerP2",(const char *)"Q"               , 50552, PARAM_TYPE_U32,   4},
    { 19,  (const char *)"PowerFactorP2",  (const char *)"PF"              , 50564, PARAM_TYPE_U32,   4},
    { 20,  (const char *)"ActivePowerP3",  (const char *)"P"               , 50548, PARAM_TYPE_U32,   4},
    { 21,  (const char *)"ReactivePowerP3",(const char *)"Q"               , 50554, PARAM_TYPE_U32,   4},
    { 22,  (const char *)"PowerFactorP3",  (const char *)"PF"              , 50566, PARAM_TYPE_U32,   4},
    { 23,  (const char *)"ApparentPower",  (const char *)"VA"              , 50540, PARAM_TYPE_U32,   4},
    { 24,  (const char *)"ActivePowerh",   (const char *)"kWh"             , 50770, PARAM_TYPE_U32,   4},
    { 25,  (const char *)"ReactivePowerh", (const char *)"varh"            , 50772, PARAM_TYPE_U32,   4},
    { 26,  (const char *)"ApparentPowerh", (const char *)"VAh"             , 50774, PARAM_TYPE_U32,   4},
    { 27,  (const char *)"ApparentPowerP1",(const char *)"VA / 0.1"        , 50556, PARAM_TYPE_U32,   4},
    { 28,  (const char *)"ApparentPowerP2",(const char *)"VA / 0.1"        , 50558, PARAM_TYPE_U32,   4},
    { 29,  (const char *)"ApparentPowerP3",(const char *)"VA / 0.1"        , 50560, PARAM_TYPE_U32,   4},
    { 30,  (const char *)"VoltageP1",      (const char *)"V / 100"         , 50514, PARAM_TYPE_U32,   4},
    { 31,  (const char *)"VoltageP2",      (const char *)"V / 100"         , 50516, PARAM_TYPE_U32,   4},
    { 32,  (const char *)"VoltageP3",      (const char *)"V / 100"         , 50518, PARAM_TYPE_U32,   4},
    { 33,  (const char *)"CurrentP1",      (const char *)"A / 1000"        , 50528, PARAM_TYPE_U32,   4},
    { 34,  (const char *)"CurrentP2",      (const char *)"A / 1000"        , 50530, PARAM_TYPE_U32,   4},
    { 35,  (const char *)"CurrentP3",      (const char *)"A / 1000"        , 50532, PARAM_TYPE_U32,   4},
    { 36,  (const char *)"ImportEnergy",   (const char *)"watt/hours/0.001", 50770, PARAM_TYPE_U32,   4},
    { 37,  (const char *)"ExportEnergy",   (const char *)"watt/hours/0.001", 50776, PARAM_TYPE_U32,   4},
};

const uint16_t num_device_parameters = (sizeof(countis_e53)/sizeof(countis_e53[0]));

uint16_t modbus_crc(uint8_t * buf, unsigned length) {
    static const uint16_t crctable[] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
    0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
    0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
    0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
    0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
    0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
    0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
    0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
    0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
    0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
    0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
    0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
    0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
    0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
    0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
    0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
    0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040 };

    uint16_t crc = 0xffff;
    while(length--) {
        uint8_t nTemp = *buf++ ^ crc;
        crc >>= 8;
        crc ^= crctable[nTemp];
    }
    return crc;
}


static uint8_t modbuspacket[MAX_MODBUS_PACKET_SIZE];


// Read characteristic value from Modbus parameter according to description table
static esp_err_t sense_modbus_read_value(uint16_t cid, void *value, uint8_t value_size)
{
    dev_parameter_descriptor_t cid_info = countis_e53[cid];

    if (value_size > cid_info.param_size) {
        ERROR_PRINTF("Parameter %s is too small %"PRIu8" for %"PRIu8, cid_info.param_key, cid_info.param_size, value_size);
        return ESP_FAIL;
    }

    uint16_t addr = cid_info.mb_reg_start;
    uint8_t count = value_size / 2;

    if (!count)
        count = 1;

    /* ADU Header (Application Data Unit) */
    modbuspacket[0] = E53_ADDR;
    /* ====================================== */
    /* PDU payload (Protocol Data Unit) */
    modbuspacket[1] = READ_HOLDING_FUNC; /*Holding*/
    modbuspacket[2] = addr >> 8;   /*Register read address */
    modbuspacket[3] = addr & 0xFF;
    modbuspacket[4] = 0; /*Register read count */
    modbuspacket[5] = count;
    /* ====================================== */
    /* ADU Tail */
    uint16_t crc = modbus_crc(modbuspacket, 6);
    modbuspacket[6] = crc & 0xFF;
    modbuspacket[7] = crc >> 8;

    if (uart_write_bytes(DEVS_UART, (char*)modbuspacket, 8) != 8) {
        ERROR_PRINTF("Failed to write out Modbus packet.");
        return ESP_FAIL;
    }

    size_t length = 0;

    for(unsigned i = 0; i < 100 && length < MAX_MODBUS_PACKET_SIZE; i++) {
        if (length) {
            uint8_t v;
            int r = uart_read_bytes(DEVS_UART, &v, 1, 0);
            if (r < 0) {
                ERROR_PRINTF("Failed to read (%zu) data from modbus", length);
                return ESP_FAIL;
            }
            else if (r > 0 && v) {
                // Ok, non zero, so it's should be a frame start.
                modbuspacket[length++] = v;
                i = 80;
            }
        }
        else {
            size_t toread;
            esp_err_t err = uart_get_buffered_data_len(DEVS_UART, &toread);
            if (err != ESP_OK) {
                ERROR_PRINTF("Failed to read length from modbus : %s(%u)", esp_err_to_name(err), (unsigned)err);
                return err;
            }

            if (toread) {
                toread = (toread > (MAX_MODBUS_PACKET_SIZE - length))?(MAX_MODBUS_PACKET_SIZE - length):toread;
                int r = uart_read_bytes(DEVS_UART, &modbuspacket[length], toread, 0);
                if (r < 0) {
                    ERROR_PRINTF("Failed to read (%zu) data from modbus", length);
                    return ESP_FAIL;
                }
                else if ( r > 0 )
                    length += toread;
            }
            else vTaskDelay(1);
        }
    }

    if (length < MIN_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus didn't return enough to be packet (%zu < %u)", length, (unsigned)MIN_MODBUS_PACKET_SIZE);
        return ESP_FAIL;
    }

    if (length > MAX_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus returned too much to be expected packet (%zu > %u)", length, (unsigned)MAX_MODBUS_PACKET_SIZE);
        return ESP_FAIL;
    }

    crc = modbus_crc(modbuspacket, length - 2);

    while(1) {
        if ( (modbuspacket[length-1] == (crc >> 8)) &&
             (modbuspacket[length-2] == (crc & 0xFF)) )
             break;

        if (!modbuspacket[length-1] && (length > MIN_MODBUS_PACKET_SIZE) ) {
            INFO_PRINTF("Trimming zero trailing message.");
            length -= 1;
            continue;
        }
        ERROR_PRINTF("CRC error with reply.");
        return ESP_FAIL;
    }

    uint8_t func = modbuspacket[1];

    if (func == READ_HOLDING_FUNC) {
        /* Yer! */

        /* For reading a holding, 16bit register
         *  https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)
         *  http://www.simplymodbus.ca/FC03.htm
         */
        uint8_t bytes_count = modbuspacket[2];
        if ((bytes_count / 2) != count) {
            ERROR_PRINTF("Modbus responsed with different count of values than requested.");
            return ESP_FAIL;
        }

        uint8_t * dst_value = (uint8_t*)value;

        if (cid_info.param_type == PARAM_TYPE_U8) {
            // Take it the order it comes
            for(unsigned n = 0; n < bytes_count; n ++)
                dst_value[n] = modbuspacket[3 + n];
        }
        else if (cid_info.param_type == PARAM_TYPE_ASCII) {
            for(unsigned n = 0; n < bytes_count; n+=2) {
                dst_value[n] = modbuspacket[3 + n + 1];
                dst_value[n+1] = modbuspacket[3 + n];
            }
        }
        else {
            // Change to little endian from big.
            for(unsigned n = 0; n < bytes_count; n ++)
                dst_value[n] = modbuspacket[3 + (value_size - n - 1)];
        }
    }
    else if (func == (READ_HOLDING_FUNC | MODBUS_ERROR_MASK)) {
        if (length > MIN_MODBUS_PACKET_SIZE) {
            ERROR_PRINTF("Slave addr:%"PRIu16" responsed with modbus exception : %"PRIu8, addr, modbuspacket[2]);
            return ESP_FAIL;
        }
        else {
            ERROR_PRINTF("Slave addr:%"PRIu16" responsed with modbus error but no exception code.", addr);
            return ESP_FAIL;
        }
    }
    else {
        ERROR_PRINTF("Unsupported function (%"PRIu8")in response.", func);
        return ESP_FAIL;
    }

    return ESP_OK;
}


static void smart_switch_switch() {
    // Switch UART to Smart Meter
    gpio_set_level(SW_SEL, 1);
    ESP_ERROR_CHECK(uart_set_mode(DEVS_UART, UART_MODE_RS485_HALF_DUPLEX));
    // Flush the input buffer from anything from other device.
    vTaskDelay(1 / portTICK_PERIOD_MS);
    uart_flush(DEVS_UART);
}


esp_err_t smart_meter_setup() {

    if(!get_socoen())
        return ESP_OK;
    DEBUG_PRINTF("Init Smart Meter");

    smart_switch_switch();

    // Make sure we're actually connected to a SOCOMEC smart meter!
    // This function really returns a null byte between each character, "S\0O\0C\0O\0".
    char soco[8];
    esp_err_t err = sense_modbus_read_value(4, soco, sizeof(soco));
    if (err != ESP_OK)
        return ESP_FAIL;

    if(soco[0] != 'S')
        goto unknown_device;
    if(soco[1] != '\0')
        goto unknown_device;
    if(soco[2] != 'O')
        goto unknown_device;
    if(soco[3] != '\0')
        goto unknown_device;
    if(soco[4] != 'C')
        goto unknown_device;
    if(soco[5] != '\0')
        goto unknown_device;
    if(soco[6] != 'O')
        goto unknown_device;
    if(soco[7] != '\0')
        goto unknown_device;

    // Try and identify the model number. This doesn't work because of
    // Endianness mismatch and also buffer overruns; Might take another look later.

    uint16_t product_order_id;
    uint16_t product_id;
    const char * prod = 0;
    const char * prodorder = 0;

    err = sense_modbus_read_value(6, &product_order_id, sizeof(product_order_id));
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to read Product Order ID.");
        return ESP_FAIL;
    }
    err = sense_modbus_read_value(7, &product_id,       sizeof(product_id));
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to read Product ID.");
        return ESP_FAIL;
    }

    if(product_order_id == 100)
        prodorder = "Countis";
    if(product_order_id == 200)
        prodorder = "Protection";
    if(product_order_id == 300)
        prodorder = "Atys";
    if(product_order_id == 400)
        prodorder = "Diris";

    if(!prodorder)
        goto unknown_device;

    if(product_id == 100)
        prod = "E53";
    if(product_id == 1000)
        prod = "ATS3";

    if(!prod) goto unknown_device;

    INFO_PRINTF("Connected to a Socomec %s %s.", prodorder, prod);
    sococonnected = 1;
    return ESP_OK;

unknown_device:
    ERROR_PRINTF("product_order_id = %d\nproduct_id = %d", product_order_id, product_id);
    for(int i = 0; i < 8; i++) {
        DEBUG_PRINTF("soco[%d] == '%c';", i, soco[i]);
    }
    ERROR_PRINTF("I don't know what this means, but it probably means that I'm not connected to a Socomec brand smart meter.");
    sococonnected = 0;
    return ESP_FAIL;
}

void get_pfleadlag() {
    int32_t powerfactor = 0;
    sense_modbus_read_value(16, &powerfactor,   sizeof(powerfactor));
    if(powerfactor > 0)
        stats_enqueue_sample(parameter_pfleadlag, 1000);
    else if(powerfactor < 0)
        stats_enqueue_sample(parameter_pfleadlag, -1000);
    else
        stats_enqueue_sample(parameter_pfleadlag, 0);
}

void get_powerfactor() {
    int32_t powerfactor = 0;
    sense_modbus_read_value(16, &powerfactor,   sizeof(powerfactor));
    if(powerfactor > 0)
        stats_enqueue_sample(parameter_pfleadlag, powerfactor);
    else if(powerfactor < 0)
        stats_enqueue_sample(parameter_pfleadlag, -powerfactor);
    else
        stats_enqueue_sample(parameter_pfleadlag, 0);
}

void get_voltage1() {
    uint32_t voltage;
    sense_modbus_read_value(30, &voltage,      sizeof(voltage));
    stats_enqueue_sample(parameter_voltage1, voltage);
}

void get_voltage2() {
    uint32_t voltage;
    sense_modbus_read_value(31, &voltage,      sizeof(voltage));
    stats_enqueue_sample(parameter_voltage2, voltage);
}

void get_voltage3() {
    uint32_t voltage;
    sense_modbus_read_value(32, &voltage,      sizeof(voltage));
    stats_enqueue_sample(parameter_voltage3, voltage);
}

void get_current1() {
    uint32_t current;
    sense_modbus_read_value(33, &current,      sizeof(current));
    stats_enqueue_sample(parameter_current1, current);
}

void get_current2() {
    uint32_t current;
    sense_modbus_read_value(34, &current,      sizeof(current));
    stats_enqueue_sample(parameter_current2, current);
}

void get_current3() {
    uint32_t current;
    sense_modbus_read_value(35, &current,      sizeof(current));
    stats_enqueue_sample(parameter_current3, current);
}
