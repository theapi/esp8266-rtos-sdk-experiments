
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "rom/ets_sys.h"
#include "crc.h"
#include "user_main.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "denbit.h"

#define UART_BUF_SIZE (1024)

char uart_buffer[UART_BUF_SIZE];

static const char *TAG = "receiver";

static xQueueHandle app_espnow_queue;

TaskHandle_t redBlinkTaskHandle;

uint32_t count = 0;

void redBlinkTask(void *pvParameters)
{
    while(1) {
        gpio_set_level(DENBIT_RED, 1);
        vTaskDelay(100 / portTICK_RATE_MS);
        gpio_set_level(DENBIT_RED, 0);
        // Suspend ourselves.
        vTaskSuspend( NULL );
    }
}

static esp_err_t app_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "WiFi started");
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* WiFi should start before using ESPNOW */
static esp_err_t app_wifi_init(void)
{
    tcpip_adapter_init();

    if (esp_event_loop_init(app_event_handler, NULL) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_event_loop_init");
      return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_init");
      return ESP_FAIL;
    }

    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_storage");
      return ESP_FAIL;
    }

    if (esp_wifi_set_mode(ESPNOW_WIFI_MODE) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_mode");
      return ESP_FAIL;
    }

    if (esp_wifi_start() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_start");
      return ESP_FAIL;
    }

    /* In order to simplify example, channel is set after WiFi started.
     * This is not necessary in real application if the two devices have
     * been already on the same channel.
     */
    if (esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, 0) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_channel");
      return ESP_FAIL;
    }

    return ESP_OK;
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void app_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    app_espnow_event_t evt;
    app_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = APP_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        esp_restart();
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(app_espnow_queue, &evt, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Receive queue fail");
        free(recv_cb->data);
        esp_restart();
    }
}

/* Parse received ESPNOW data. */
int app_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, int *magic)
{
    app_espnow_data_t *buf = (app_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(app_espnow_data_t)) {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

static void app_espnow_task(void *pvParameter)
{
    app_espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    int recv_magic = 0;
    int ret;

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Start listening for broadcast data");

    while (xQueueReceive(app_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case APP_ESPNOW_RECV_CB:
            {
                app_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                // Blink the red led.
                vTaskResume( redBlinkTaskHandle );

                ret = app_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                free(recv_cb->data);
                if (ret == APP_ESPNOW_DATA_BROADCAST) {
                    //ESP_LOGI(TAG, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                    uint16_t len = sprintf(uart_buffer, "%d - broadcast data from: "MACSTR", len: %d\n", ++count, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                    // Write data back to the UART
                    uart_write_bytes(UART_NUM_0, (const char *) uart_buffer, len);
                }
                else {
                    ESP_LOGI(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}

static esp_err_t app_espnow_init(void)
{
    app_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(app_espnow_event_t));
    if (app_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register receiving callback function. */
    if (esp_now_init() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_init");
      return ESP_FAIL;
    }

    if (esp_now_register_recv_cb(app_espnow_recv_cb) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_register_send_cb");
      return ESP_FAIL;
    }

    /* Set primary master key. */
    if (esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_set_pmk");
      return ESP_FAIL;
    }

    xTaskCreate(app_espnow_task, "app_espnow_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}

void app_main()
{
    denbit_init();
    //xTaskCreate(greenBlinkTask, "greenBlinkTask", 180, NULL, 1, &greenBlinkTaskHandle);
    xTaskCreate(redBlinkTask, "redBlinkTask", 180, NULL, 1, &redBlinkTaskHandle);

    // Configure parameters of an UART driver,
    // communication pins and install the driver
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL);

    // Initialize NVS
    if (nvs_flash_init() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: nvs_flash_init");
      esp_restart();
    }

    if (app_wifi_init() != ESP_OK) {
      esp_restart();
    }
    if (app_espnow_init() != ESP_OK) {
      esp_restart();
    }
}
