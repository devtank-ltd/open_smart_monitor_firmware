/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/uart.h"
#include "esp_modbus_master.h"
#include "mqtt-sn.h"
#include "driver/gpio.h"
#include "pinmap.h"

#define UART_NUM UART_NUM_1

#define E53_ADDR 5
#define HOURMETER 50512

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

// The daft thing here, which is mandated by the freemodbus library, is that the Cid must be a number starting from zero and incremented by one each time. Essentially,
// each element needs a Cid equal to the subscript that identifies it, which makes adding ro removing parameters very tricky. Therefore, I recomment you only add them
// to the end of the list. Even though it means that the order of items ends up making no sense.
const mb_parameter_descriptor_t countis_e53[] = { \
 // { Cid, Param Name,                     Units,                           SlaveAddr,Modbus Reg Type,  Start,Size,Offs, Data Type,DataSize,ParamOptions,Access Mode}
    { 0,   (const char *)"Hour meter",     (const char *)"watt/hours /100", E53_ADDR, MB_PARAM_HOLDING, 50512,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 1,   (const char *)"Apparent power", (const char *)"VA / 0.1",        E53_ADDR, MB_PARAM_HOLDING, 50536,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 2,   (const char *)"Hour meter",     (const char *)"watt/hours /100", E53_ADDR, MB_PARAM_HOLDING, 50592,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 3,   (const char *)"Network type",   (const char *)"Network type",    E53_ADDR, MB_PARAM_HOLDING, 40448,   1,   0, PARAM_TYPE_U8,    1, NOOPTS, PAR_PERMS_READ },
    { 4,   (const char *)"Ident",          (const char *)"Should read SOCO",E53_ADDR, MB_PARAM_HOLDING, 50000,   8,   0, PARAM_TYPE_ASCII, 8, NOOPTS, PAR_PERMS_READ },
    { 5,   (const char *)"Vendor name",    (const char *)"",                E53_ADDR, MB_PARAM_HOLDING, 50042,   8,   0, PARAM_TYPE_ASCII, 8, NOOPTS, PAR_PERMS_READ },
    { 6,   (const char *)"ProductOrderID", (const char *)"",                E53_ADDR, MB_PARAM_HOLDING, 50004,   8,   0, PARAM_TYPE_U16,   8, NOOPTS, PAR_PERMS_READ },
    { 7,   (const char *)"ProductID",      (const char *)"",                E53_ADDR, MB_PARAM_HOLDING, 50005,   8,   0, PARAM_TYPE_U16,   8, NOOPTS, PAR_PERMS_READ },
    { 8,   (const char *)"Voltage",        (const char *)"V",               E53_ADDR, MB_PARAM_HOLDING, 50520,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 9,   (const char *)"Frequency",      (const char *)"Hz",              E53_ADDR, MB_PARAM_HOLDING, 50606,   4,   0, PARAM_TYPE_FLOAT, 4, NOOPTS, PAR_PERMS_READ },
    { 10,  (const char *)"Current",        (const char *)"A",               E53_ADDR, MB_PARAM_HOLDING, 50528,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 11,  (const char *)"ActivePower",    (const char *)"P",               E53_ADDR, MB_PARAM_HOLDING, 50536,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 12,  (const char *)"ReactivePower",  (const char *)"Q",               E53_ADDR, MB_PARAM_HOLDING, 50538,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 13,  (const char *)"PowerFactor",    (const char *)"PF",              E53_ADDR, MB_PARAM_HOLDING, 50542,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },

    { 14,  (const char *)"ActivePowerP1",  (const char *)"P",               E53_ADDR, MB_PARAM_HOLDING, 50544,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 15,  (const char *)"ReactivePowerP1",(const char *)"Q",               E53_ADDR, MB_PARAM_HOLDING, 50550,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 16,  (const char *)"PowerFactorP1",  (const char *)"PF",              E53_ADDR, MB_PARAM_HOLDING, 50562,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },

    { 17,  (const char *)"ActivePowerP2",  (const char *)"P",               E53_ADDR, MB_PARAM_HOLDING, 50546,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 18,  (const char *)"ReactivePowerP2",(const char *)"Q",               E53_ADDR, MB_PARAM_HOLDING, 50552,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 19,  (const char *)"PowerFactorP2",  (const char *)"PF",              E53_ADDR, MB_PARAM_HOLDING, 50564,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },

    { 20,  (const char *)"ActivePowerP3",  (const char *)"P",               E53_ADDR, MB_PARAM_HOLDING, 50548,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 21,  (const char *)"ReactivePowerP3",(const char *)"Q",               E53_ADDR, MB_PARAM_HOLDING, 50554,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 22,  (const char *)"PowerFactorP3",  (const char *)"PF",              E53_ADDR, MB_PARAM_HOLDING, 50566,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 23,  (const char *)"ApparentPower",  (const char *)"VA",              E53_ADDR, MB_PARAM_HOLDING, 50540,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },

    { 24,  (const char *)"ActivePowerh",   (const char *)"kWh",             E53_ADDR, MB_PARAM_HOLDING, 50770,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 25,  (const char *)"ReactivePowerh", (const char *)"varh",            E53_ADDR, MB_PARAM_HOLDING, 50772,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ },
    { 26,  (const char *)"ApparentPowerh", (const char *)"VAh",             E53_ADDR, MB_PARAM_HOLDING, 50774,   4,   0, PARAM_TYPE_U32,   4, NOOPTS, PAR_PERMS_READ }
};

const uint16_t num_device_parameters = (sizeof(countis_e53)/sizeof(countis_e53[0]));

// Read characteristic value from Modbus parameter according to description table
esp_err_t sense_modbus_read_value(uint16_t cid, void *value)
{
    uint8_t type = 0;
    mb_parameter_descriptor_t cid_info = countis_e53[cid];

    // Send Modbus request to read cid correspond registers
    esp_err_t error = mbc_master_get_parameter(cid, (char*)cid_info.param_key, value, &type);
    SENSE_MB_CHECK((type <= PARAM_TYPE_ASCII), ESP_ERR_NOT_SUPPORTED, "returned data type is not supported (%u)", type);
    return error;
}

esp_err_t init_smart_meter() {

    mb_communication_info_t comm = {
            .port = UART_NUM_1,
            .mode = MB_MODE_RTU,
            .baudrate = 9600,
            .parity = UART_PARITY_DISABLE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    SENSE_MB_CHECK((master_handler != NULL), ESP_ERR_INVALID_STATE,
                                "mb controller initialization fail.");
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_start();
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);
    vTaskDelay(5);
    err = mbc_master_set_descriptor(&countis_e53[0], num_device_parameters);
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                "mb controller set descriptor fail, returns(0x%x).",
                                (uint32_t)err);

    if(uart_set_pin(UART_NUM, HPM_UART_TX, HPM_UART_RX, RS485_DE, UART_PIN_NO_CHANGE) == ESP_FAIL)
        printf("Error in uart_set_pin!\n");
    uart_set_mode(UART_NUM, UART_MODE_RS485_HALF_DUPLEX);


    // Make sure we're actually connected to a SOCOMEC smart meter!
    // This function really returns a null byte between each character, "S\0O\0C\0O\0".
    char soco[8];
    sense_modbus_read_value(4, soco);

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

    sense_modbus_read_value(6, &product_order_id);
    sense_modbus_read_value(7, &product_id);

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

    printf("Connected to a Socomec %s %s.\n", prodorder, prod);
    sococonnected = 1;
    return err;

unknown_device:
    printf("product_order_id = %d\nproduct_id = %d\n", product_order_id, product_id);
    for(int i = 0; i < 8; i++) printf("soco[%d] == '%c';\n", i, soco[i]);
    printf("I don't know what this means, but it probably means that I'm not connected to a Socomec brand smart meter.\n");
    return err;

}

void query_countis()
{
    if(!sococonnected)
       return;
    gpio_set_level(SW_SEL, 1);
    uint32_t hourmeter = 0;
    uint32_t apparentpower = 0;
    float fhourmeter = 0.0;
    uint8_t networktype = 0;
    uint32_t volt = 0;
    float ffreq = 0.0;
    uint32_t current = 0;

    sense_modbus_read_value(0,  &hourmeter);
    sense_modbus_read_value(1,  &apparentpower);
    sense_modbus_read_value(2,  &fhourmeter);
    sense_modbus_read_value(3,  &networktype);
    sense_modbus_read_value(8,  &volt);
    sense_modbus_read_value(9,  &ffreq);
    sense_modbus_read_value(10, &current);

    // The volt reads as:
    // high byte, low byte, zero, zero
    // all inside a uint32_t. The two upper bytes encode a number a hundred times the actual voltage.
    int mV = (volt >> 16) * 100;
    // I suspect a similar thing goes for the ampage.
    int mA = (current >> 16) * 100;

    const char * ntnames[] = { "1bl", "2bl", "3bl", "3nbl", "4bl", "4nbl" };

    printf("network type: %s\n", ntnames[networktype]);

    printf("hourmeter = i%u f%f\napparent_power = %u\n", hourmeter, fhourmeter, apparentpower);
    printf("%dmV, %dmA\n", mV, mA);

    for(int i = 8; i <= 26; i++) {
        int32_t v = 0;
        sense_modbus_read_value(i, &v);
        mqtt_announce_int(countis_e53[i].param_key, v >> 16);
        printf("%s = %d\n", countis_e53[i].param_key, v);
    }
}
