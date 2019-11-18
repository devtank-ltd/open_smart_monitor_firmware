#include "driver/gpio.h"
#include "pinmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static int lvl;
static xQueueHandle gpio_evt_queue = NULL;
static int volume = 0;

static void IRAM_ATTR isr(void * arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    printf("started the task which counts the pulses\n");
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            int nlvl = gpio_get_level(io_num);
            if(lvl != nlvl) {
                if(lvl)
                    volume++;
                lvl = nlvl;
            }
        }   
    }   
}

void volume_setup() {
    printf("Setting the volume measurement gpio up\n");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << FLOWRATE_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;

    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(FLOWRATE_GPIO, isr, (void*) FLOWRATE_GPIO);
}

void qry_volume() {
    printf("volume = %d\n", volume);
}
