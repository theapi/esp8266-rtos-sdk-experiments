

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_system.h"

#include "denbit/denbit.h"

void app_main(void)
{
    denbit_config();

    int cnt = 0;
    while (1) {
        vTaskDelay(500 / portTICK_RATE_MS);
        gpio_set_level(DENBIT_GREEN, cnt % 2);
        gpio_set_level(DENBIT_RGB_GREEN, cnt % 2);
        cnt++;
        gpio_set_level(DENBIT_RED, cnt % 2);
        gpio_set_level(DENBIT_RGB_RED, cnt % 2);
        //printf("cnt:%i\n", cnt);
    }
}

