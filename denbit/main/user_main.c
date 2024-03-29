

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_sleep.h"

#include "denbit.h"

void blinkTask(void *pvParameters)
{
    int cnt = 0;
    while(1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(DENBIT_GREEN, cnt % 2);
        gpio_set_level(DENBIT_RGB_GREEN, cnt % 2);
        cnt++;
        gpio_set_level(DENBIT_RED, cnt % 2);
        gpio_set_level(DENBIT_RGB_RED, cnt % 2);

        if (cnt > 10) {
          // Connect GPIO16 to RESET (after flashing for this to work)
          esp_deep_sleep(10e6);
        }
    }
}

void app_main(void)
{
    denbit_config();

    xTaskCreate(blinkTask, "blinkTask", 180, NULL, 2, NULL);

    // int cnt = 0;
    // while (1) {
    //     vTaskDelay(1000 / portTICK_RATE_MS);
    //     gpio_set_level(DENBIT_GREEN, cnt % 2);
    //     gpio_set_level(DENBIT_RGB_GREEN, cnt % 2);
    //     cnt++;
    //     gpio_set_level(DENBIT_RED, cnt % 2);
    //     gpio_set_level(DENBIT_RGB_RED, cnt % 2);
    //     //printf("cnt:%i\n", cnt);
    // }
}

