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

#define UART_NUM UART_NUM_1
#define HPM_UART_TX 17
#define HPM_UART_RX 16
#define RS485_DE 14

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
    { 6,   (const char *)"Product name",   (const char *)"",                E53_ADDR, MB_PARAM_HOLDING, 50050,   8,   0, PARAM_TYPE_ASCII, 8, NOOPTS, PAR_PERMS_READ },
    { 7,   (const char *)"ProductNameExt", (const char *)"",                E53_ADDR, MB_PARAM_HOLDING, 50058,   8,   0, PARAM_TYPE_ASCII, 8, NOOPTS, PAR_PERMS_READ },
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

    if(soco[0] != 'S')     goto unknown_device;
    if(soco[1] !=    '\0') goto unknown_device;
    if(soco[2] != 'O')     goto unknown_device;
    if(soco[3] !=    '\0') goto unknown_device;
    if(soco[4] != 'C')     goto unknown_device;
    if(soco[5] !=    '\0') goto unknown_device;
    if(soco[6] != 'O')     goto unknown_device;
    if(soco[7] !=    '\0') goto unknown_device;

    return err;
    // Try and identify the model number. This doesn't work because of
    // Endianness mismatch and also buffer overruns; Might take another look later.
    /*
    char vendor[8];
    char productname[8];
    char productnameext[8];
    vendor[7] = '\0';
    productname[7] = '\0';
    productnameext[7] = '\0';

    sense_modbus_read_value(5, vendor);
    sense_modbus_read_value(6, productname);
    sense_modbus_read_value(7, productnameext);

    printf("Connected to a %s %s %s.\n", vendor, productname, productnameext);
    */
unknown_device:
    for(int i = 0; i < 8; i++) printf("soco[%d] == '%c';\n", i, soco[i]);
    printf("I don't know what this means, but it probably means that I'm not connected to a Socomec brand smart meter.");
    return err;

}

void query_countis()
{
    uint32_t hourmeter = 0;
    uint32_t apparentpower = 0;
    float fhourmeter = 0.0;
    uint8_t networktype = 0;

    sense_modbus_read_value(0, &hourmeter);
    sense_modbus_read_value(1, &apparentpower);
    sense_modbus_read_value(2, &fhourmeter);
    sense_modbus_read_value(3, &networktype);

    const char * ntnames[] = { "1bl", "2bl", "3bl", "3nbl", "4bl", "4nbl" };

    printf("network type: %s\n", ntnames[networktype]);

    printf("hourmeter = i%u f%f\napparent_power = %u\n", hourmeter, fhourmeter, apparentpower);
}
