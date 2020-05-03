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

#define E53_ADDR 5

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

uint16_t modbus_crc(uint8_t * buf, unsigned length) {
    static const uint16_t crctable[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };

    uint16_t crc = 0xffff;
    while(length--) {
        uint8_t nTemp = *buf++ ^ crc;
        crc >>= 8;
        crc ^= crctable[nTemp];
    }
    return crc;
}


// Read characteristic value from Modbus parameter according to description table
static esp_err_t sense_modbus_read_value(uint16_t cid, void *value, uint8_t value_size)
{
    dev_parameter_descriptor_t cid_info = countis_e53[cid];

    if (value_size > cid_info.param_size) {
        ERROR_PRINTF("Parameter %s is too small %"PRIu8" for %"PRIu8, cid_info.param_key, cid_info.param_size, value_size);
        return ESP_FAIL;
    }

    uint8_t packet[READ_HOLDING_REQ_PACKET_SIZE];
    uint16_t addr = cid_info.mb_reg_start;
    uint8_t count = value_size / 2;
    esp_err_t err;

    if (!count)
        count = 1;

    /* ADU Header (Application Data Unit) */
    packet[0] = E53_ADDR;

    /* PDU payload (Protocol Data Unit) */
    packet[1] = READ_HOLDING_FUNC; /*Holding*/
    packet[2] = addr >> 8;   /*Register read address */
    packet[3] = addr & 0xFF;
    packet[4] = 0; /*Register read count */
    packet[5] = count;

    /* ADU Tail */
    uint16_t crc = modbus_crc(packet, sizeof(packet) - 2);
    packet[6] = crc & 0xFF;
    packet[7] = crc >> 8;

    /* 28 bit break after writing*/
    if (uart_write_bytes_with_break(DEVS_UART, (char*)packet, sizeof(packet), 28) != sizeof(packet)) {
        ERROR_PRINTF("Failed to write out Modbus packet.");
        return ESP_FAIL;
    }

    /* Give 50ms (bit long, maybe should be async) for the answer to come.*/
    vTaskDelay(50 / portTICK_PERIOD_MS);

    size_t length = 0;
    err = uart_get_buffered_data_len(DEVS_UART, &length);
    if (err != ESP_OK) {
        ERROR_PRINTF("Failed to read anything from modbus : %s(%u)", esp_err_to_name(err), (unsigned)err);
        return err;
    }

    if (length < MIN_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus didn't return enough to be packet (%zu < %u)", length, (unsigned)MIN_MODBUS_PACKET_SIZE);
        return ESP_FAIL;
    }

    if (length > MAX_MODBUS_PACKET_SIZE) {
        ERROR_PRINTF("Modbus returned too much to be expected packet (%zu > %u)", length, (unsigned)MAX_MODBUS_PACKET_SIZE);
        return ESP_FAIL;
    }

    uint8_t reply[length];

    int r = uart_read_bytes(DEVS_UART, reply, length, 0);
    if (r < 0) {
        ERROR_PRINTF("Failed to read (%zu) data from modbus", length);
        return ESP_FAIL;
    }
    else if (r != length) {
        ERROR_PRINTF("Read length(%d) from modbus isn't expected length(%zu)", r, length);
        return ESP_FAIL;
    }

    // Skip over any leading zeros
    unsigned start = 0;
    for(unsigned n = 0; n < length; n++)
        if (reply[n]) {
            start = n;
            break;
        }

    crc = modbus_crc(reply + start, length - start - 2);

    while(1) {
        if ( (reply[length-1] == (crc >> 8)) &&
             (reply[length-2] == (crc & 0xFF)) )
             break;

        if (!reply[length-1] && (length > MIN_MODBUS_PACKET_SIZE) ) {
            INFO_PRINTF("Trimming zero trailing message.");
            length -= 1;
            continue;
        }
        ERROR_PRINTF("CRC error with reply.");
        return ESP_FAIL;
    }

    uint8_t func = reply[start + 1];

    if (func == READ_HOLDING_FUNC) {
        /* Yer! */

        /* For reading a holding, 16bit register
         *  https://en.wikipedia.org/wiki/Modbus#Modbus_RTU_frame_format_(primarily_used_on_asynchronous_serial_data_lines_like_RS-485/EIA-485)
         *  http://www.simplymodbus.ca/FC03.htm
         */
        uint8_t bytes_count = reply[ start + 2];
        if ((bytes_count / 2) != count) {
            ERROR_PRINTF("Modbus responsed with different count of values than requested.");
            return ESP_FAIL;
        }

        uint8_t * dst_value = (uint8_t*)value;

        if (cid_info.param_type == PARAM_TYPE_U8) {
            // Take it the order it comes
            for(unsigned n = 0; n < bytes_count; n ++)
                dst_value[n] = reply[start + 3 + n];
        }
        else if (cid_info.param_type == PARAM_TYPE_ASCII) {
            for(unsigned n = 0; n < bytes_count; n+=2) {
                dst_value[n] = reply[start + 3 + n + 1];
                dst_value[n+1] = reply[start + 3 + n];
            }
        }
        else {
            // Change to little endian from big.
            for(unsigned n = 0; n < bytes_count; n ++)
                dst_value[n] = reply[start + 3 + (value_size - n - 1)];
        }
    }
    else if (func == (READ_HOLDING_FUNC | MODBUS_ERROR_MASK)) {
        if (length > MIN_MODBUS_PACKET_SIZE) {
            ERROR_PRINTF("Slave responsed with modbus exception : %"PRIu8, reply[start + 2]);
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
