/**
 * Basic SPI_master example
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp8266/spi_struct.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/spi.h"

#define GPIO_OUTPUT_IO_0    2
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)


static const char *TAG = "spi_master_example";

// Write an 8-bit data
static esp_err_t write_byte(uint8_t data)
{
    uint32_t buf = data << 24;
    spi_trans_t trans = {0};
    trans.mosi = &buf;
    trans.bits.mosi = 8;
    spi_trans(HSPI_HOST, trans);
    return ESP_OK;
}

static void spi_master_write_slave_task(void *arg)
{
    uint8_t display = 1;
    while (1) {
        if (display == 0) {
          display = 1;
        }
        write_byte(display);
        display = display << 1;
        //ESP_LOGI(TAG, "Count: %d", count);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void blinkTask(void *pvParameters)
{
    int cnt = 0;
    while(1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(GPIO_OUTPUT_IO_0, ++cnt % 2);
    }
}

void setupBlink() {
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
    xTaskCreate(blinkTask, "blinkTask", 1024, NULL, 2, NULL);
}

void setupSPI() {
    ESP_LOGI(TAG, "init spi");
    spi_config_t spi_config;
    // Load default interface parameters
    // CS_EN:1, MISO_EN:1, MOSI_EN:1, BYTE_TX_ORDER:1, BYTE_TX_ORDER:1, BIT_RX_ORDER:0, BIT_TX_ORDER:0, CPHA:0, CPOL:0
    spi_config.interface.val = SPI_DEFAULT_INTERFACE;
    spi_config.mode = SPI_MASTER_MODE;
    // Set the SPI clock frequency division factor
    spi_config.clk_div = SPI_10MHz_DIV;
    // Cancel MISO
    spi_config.interface.miso_en = 0;
    spi_init(HSPI_HOST, &spi_config);

    // create spi_master_write_slave_task
    xTaskCreate(spi_master_write_slave_task, "spi_master_write_slave_task", 2048, NULL, 10, NULL);
}

void app_main(void)
{
    // Configure the GPIO before the SPI.
    setupBlink();

    setupSPI();
}
