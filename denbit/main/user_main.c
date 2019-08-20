

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_system.h"

#define DENBIT_GREEN     13 // D7
#define DENBIT_RED       12 // D6
#define DENBIT_RGB_RED   15 // D8
#define DENBIT_RGB_GREEN 14 // D5
#define DENBIT_RGB_BLUE  10 // SD3
#define DENBIT_SW1 2
#define DENBIT_SW2 16

// #define GPIO_OUTPUT_IO_0    2
//#define GPIO_OUTPUT_PIN_SEL ( (1ULL<<DENBIT_GREEN) | (1ULL<<DENBIT_RED) | (1ULL<<DENBIT_RGB_RED) | (1ULL<<DENBIT_RGB_GREEN) | (1ULL<<DENBIT_RGB_BLUE))
#define GPIO_OUTPUT_PIN_SEL ( (1ULL<<DENBIT_GREEN) | (1ULL<<DENBIT_RED) | (1ULL<<DENBIT_RGB_RED) | (1ULL<<DENBIT_RGB_GREEN) | (1ULL<<DENBIT_RGB_BLUE))

void denbit_config() {
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

void app_main(void)
{
    denbit_config();
    gpio_set_level(DENBIT_RGB_RED, 0);
    gpio_set_level(DENBIT_RGB_GREEN, 0);
    gpio_set_level(DENBIT_RGB_BLUE, 0);

    int cnt = 0;

    while (1) {
        vTaskDelay(500 / portTICK_RATE_MS);
        cnt++;
        gpio_set_level(DENBIT_GREEN, cnt % 2);
        gpio_set_level(DENBIT_RED, cnt % 2);
        //printf("cnt:%i\n", cnt);
    }
}

