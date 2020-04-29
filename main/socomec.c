/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/uart.h"
#include "mqtt-sn.h"
#include "driver/gpio.h"
#include "pinmap.h"
#include "logging.h"

#define READ_HOLDING_FUNC 3
#define MODBUS_ERROR_MASK 0x80
#define READ_HOLDING_BASE 40001

#define E53_ADDR 5
#define HOURMETER 50512

/*         <               ADU                         >
            addr(1), func(1), reg(2), count(2) , crc(2)
                             <      PDU       >

    For reading a holding, PDU is 16bit register address and 16bit register count.
    https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)
*/
#define READ_HOLDING_REQ_PACKET_SIZE  (1 + 1 + 2 + 2 + 2)
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
    { 2,   (const char *)"Hour meter",     (const char *)"watt/hours /100" , 50592, PARAM_TYPE_U32,   4},
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
    { 16,  (const char *)"PowerFactorP1",  (const char *)"PF"              , 50562, PARAM_TYPE_U32,   4},
    { 17,  (const char *)"ActivePowerP2",  (const char *)"P"               , 50546, PARAM_TYPE_U32,   4},
    { 18,  (const char *)"ReactivePowerP2",(const char *)"Q"               , 50552, PARAM_TYPE_U32,   4},
    { 19,  (const char *)"PowerFactorP2",  (const char *)"PF"              , 50564, PARAM_TYPE_U32,   4},
    { 20,  (const char *)"ActivePowerP3",  (const char *)"P"               , 50548, PARAM_TYPE_U32,   4},
    { 21,  (const char *)"ReactivePowerP3",(const char *)"Q"               , 50554, PARAM_TYPE_U32,   4},
    { 22,  (const char *)"PowerFactorP3",  (const char *)"PF"              , 50566, PARAM_TYPE_U32,   4},
    { 23,  (const char *)"ApparentPower",  (const char *)"VA"              , 50540, PARAM_TYPE_U32,   4},
    { 24,  (const char *)"ActivePowerh",   (const char *)"kWh"             , 50770, PARAM_TYPE_U32,   4},
    { 25,  (const char *)"ReactivePowerh", (const char *)"varh"            , 50772, PARAM_TYPE_U32,   4},
    { 26,  (const char *)"ApparentPowerh", (const char *)"VAh"             , 50774, PARAM_TYPE_U32,   4}
};

const uint16_t num_device_parameters = (sizeof(countis_e53)/sizeof(countis_e53[0]));

/*
 * Use FreeModBus's CRC16.
 * */
#define UCHAR uint8_t
#define USHORT uint16_t
static const UCHAR aucCRCHi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40
};

static const UCHAR aucCRCLo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
    0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
    0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
    0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
    0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
    0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
    0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40
};

USHORT
usMBCRC16( UCHAR * pucFrame, USHORT usLen )
{
    UCHAR           ucCRCHi = 0xFF;
    UCHAR           ucCRCLo = 0xFF;
    int             iIndex;

    while( usLen-- )
    {
        iIndex = ucCRCLo ^ *( pucFrame++ );
        ucCRCLo = ( UCHAR )( ucCRCHi ^ aucCRCHi[iIndex] );
        ucCRCHi = aucCRCLo[iIndex];
    }
    return ( USHORT )( ucCRCHi << 8 | ucCRCLo );
}



// Read characteristic value from Modbus parameter according to description table
static esp_err_t sense_modbus_read_value(uint16_t cid, void *value, uint8_t value_size)
{
    dev_parameter_descriptor_t cid_info = countis_e53[cid];

    if (value_size != cid_info.param_size) {
        ERROR_PRINTF("Parameter %s is %"PRIu8" not %"PRIu8, cid_info.param_key, cid_info.param_size, value_size);
        return ESP_FAIL;
    }

    uint8_t packet[READ_HOLDING_REQ_PACKET_SIZE];
    uint16_t addr = cid_info.mb_reg_start;
    uint8_t count = value_size / 2;

    if (!count)
        count = 1;

    addr -= READ_HOLDING_BASE;

    packet[0] = E53_ADDR;
    packet[1] = READ_HOLDING_FUNC; /*Holding*/
    packet[2] = addr >> 8;   /*Register read address */
    packet[3] = addr & 0xFF;
    packet[4] = 0; /*Register read count */
    packet[5] = count;

    uint16_t crc = usMBCRC16(packet, sizeof(packet) - 2);
    packet[6] = crc >> 8;
    packet[7] = crc & 0xFF;

    if (uart_write_bytes(DEVS_UART, (char*)packet, sizeof(packet)) != sizeof(packet)) {
        ERROR_PRINTF("Failed to write out Modbus packet.");
        return ESP_FAIL;
    }

    esp_err_t err = uart_wait_tx_done(DEVS_UART, 10 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to write all to modbus(%u) : %s", (unsigned)err, esp_err_to_name(err));
        return err;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    size_t length = 0;
    err = uart_get_buffered_data_len(DEVS_UART, &length);
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to read anything from modbus(%u) : %s", (unsigned)err, esp_err_to_name(err));
        return err;
    }

    if (length < MIN_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus didn't return enough to be packet.");
        return ESP_FAIL;
    }

    if (length > MAX_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus returned too much to be expected packet.");
        return ESP_FAIL;
    }

    uint8_t reply[length];

    err = uart_read_bytes(DEVS_UART, reply, length, 0);
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to read data from modbus(%u) : %s", (unsigned)err, esp_err_to_name(err));
        return err;
    }

    crc = usMBCRC16(reply, length - 2);

    if ( (reply[length-2] != (crc >> 8)) ||
         (reply[length-1] != (crc & 0xFF)) ) {
        ERROR_PRINTF("CRC error with reply.");
        return ESP_FAIL;
    }

    uint8_t func = reply[1];

    if (func == READ_HOLDING_FUNC) {
        /* Yer! */

        /* For reading a holding, 16bit register
         *  https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)
         */
        uint8_t numbers_of_value = reply[2];
        if (numbers_of_value != count) {
            ERROR_PRINTF("Modbus responsed with different count of values than requested.");
            return ESP_FAIL;
        }

        uint8_t * dst_value = (uint8_t*)value;

        if (cid_info.param_type == PARAM_TYPE_U8 ||
            cid_info.param_type == PARAM_TYPE_ASCII) {
            // Take it the order it comes
            for(unsigned n = 0; n < value_size; n ++)
                dst_value[n] = reply[3 + n];
        }
        else {
            // Change to little endian from big.
            for(unsigned n = 0; n < value_size; n ++)
                dst_value[n] = reply[3 + (value_size - n - 1)];
        }
    }
    else if (func == (READ_HOLDING_FUNC | MODBUS_ERROR_MASK)) {
        if (length > MIN_MODBUS_PACKET_SIZE) {
            ERROR_PRINTF("Slave responsed with modbus exception : %"PRIu8, reply[MIN_MODBUS_PACKET_SIZE-2]);
            return ESP_FAIL;
        }
        else {
            ERROR_PRINTF("Slave responsed with modbus error but no exception code.");
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
}


esp_err_t init_smart_meter() {

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
    if (err != ESP_OK)
        return ESP_FAIL;
    err = sense_modbus_read_value(7, &product_id,       sizeof(product_id));
    if (err != ESP_OK)
        return ESP_FAIL;

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
    return ESP_FAIL;

}

void query_countis()
{
    if(!sococonnected)
       return;
    smart_switch_switch();

    uint32_t hourmeter     = 0;
    uint32_t apparentpower = 0;
    float    fhourmeter    = 0.0;
    uint8_t  networktype   = 0;
    uint32_t volt          = 0;
    float    ffreq         = 0.0;
    uint32_t current       = 0;

    sense_modbus_read_value(0,  &hourmeter,     sizeof(hourmeter));
    sense_modbus_read_value(1,  &apparentpower, sizeof(apparentpower));
    sense_modbus_read_value(2,  &fhourmeter,    sizeof(fhourmeter));
    sense_modbus_read_value(3,  &networktype,   sizeof(networktype));
    sense_modbus_read_value(8,  &volt,          sizeof(volt));
    sense_modbus_read_value(9,  &ffreq,         sizeof(ffreq));
    sense_modbus_read_value(10, &current,       sizeof(current));

    // The volt reads as:
    // high byte, low byte, zero, zero
    // all inside a uint32_t. The two upper bytes encode a number a hundred times the actual voltage.
    int mV = (volt >> 16) * 100;
    // I suspect a similar thing goes for the ampage.
    int mA = (current >> 16) * 100;

    const char * ntnames[] = { "1bl", "2bl", "3bl", "3nbl", "4bl", "4nbl" };

    INFO_PRINTF("network type: %s", ntnames[networktype]);

    INFO_PRINTF("hourmeter = i%u f%f\napparent_power = %u", hourmeter, fhourmeter, apparentpower);
    INFO_PRINTF("%dmV, %dmA", mV, mA);

    for(int i = 8; i <= 26; i++) {
        int32_t v = 0;
        sense_modbus_read_value(i, &v, sizeof(v));
        mqtt_announce_int(countis_e53[i].param_key, v >> 16);
        INFO_PRINTF("%s = %d", countis_e53[i].param_key, v);
    }
}
