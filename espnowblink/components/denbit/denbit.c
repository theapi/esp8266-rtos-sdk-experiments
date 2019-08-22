
#include "driver/gpio.h"
#include "denbit.h"

void denbit_init() {
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = DENBIT_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(DENBIT_RGB_RED, 0);
    gpio_set_level(DENBIT_RGB_GREEN, 0);
    gpio_set_level(DENBIT_RGB_BLUE, 0);
    gpio_set_level(DENBIT_RED, 0);
    gpio_set_level(DENBIT_GREEN, 0);

}