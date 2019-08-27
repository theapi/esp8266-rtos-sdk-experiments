
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_system.h"

#define GPIO_OUTPUT_IO_0    2
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)


void blinkTask(void *pvParameters)
{
    int cnt = 0;
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    while(1) {
        vTaskDelay(500 / portTICK_RATE_MS);
        gpio_set_level(GPIO_OUTPUT_IO_0, ++cnt % 2);
        // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        // printf("cnt:%i\n", cnt);
        // printf("uxHighWaterMark:%lu\n", uxHighWaterMark);
    }
}


void app_main(void)
{
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

    xTaskCreate(blinkTask, "blinkTask", 180, NULL, 2, NULL);

    // int cnt = 0;
    // while (1) {
    //     vTaskDelay(500 / portTICK_RATE_MS);
    //     gpio_set_level(GPIO_OUTPUT_IO_0, ++cnt % 2);
    //     printf("cnt:%i\n", cnt);
    // }


}

