
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

#include "denbit.h"

static const char *TAG = "espnowblink";

static xQueueHandle app_espnow_queue;

static uint8_t app_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_app_espnow_seq[APP_ESPNOW_DATA_MAX] = { 0, 0 };

TaskHandle_t greenBlinkTaskHandle;
TaskHandle_t redBlinkTaskHandle;

void greenBlinkTask(void *pvParameters)
{
    while(1) {
        gpio_set_level(DENBIT_GREEN, 1);
        vTaskDelay(100 / portTICK_RATE_MS);
        gpio_set_level(DENBIT_GREEN, 0);
        // Suspend ourselves.
        vTaskSuspend( NULL );
    }
}

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

static void app_espnow_deinit(app_espnow_send_param_t *send_param);

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
static void app_wifi_init(void)
{
    tcpip_adapter_init();

    if (esp_event_loop_init(app_event_handler, NULL) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_event_loop_init");
      abort();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_init");
      abort();
    }

    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_storage");
      abort();
    }

    if (esp_wifi_set_mode(ESPNOW_WIFI_MODE) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_mode");
      abort();
    }

    if (esp_wifi_start() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_start");
      abort();
    }

    /* In order to simplify example, channel is set after WiFi started.
     * This is not necessary in real application if the two devices have
     * been already on the same channel.
     */
    if (esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, 0) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_wifi_set_channel");
      abort();
    }
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void app_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    app_espnow_event_t evt;
    app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = APP_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(app_espnow_queue, &evt, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

/* Prepare ESPNOW data to be sent. */
void app_espnow_data_prepare(app_espnow_send_param_t *send_param)
{
    app_espnow_data_t *buf = (app_espnow_data_t *)send_param->buffer;
    int i = 0;

    assert(send_param->len >= sizeof(app_espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? APP_ESPNOW_DATA_BROADCAST : APP_ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_app_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    for (i = 0; i < send_param->len - sizeof(app_espnow_data_t); i++) {
        buf->payload[i] = (uint8_t)esp_random();
    }
    buf->crc = crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void app_espnow_task(void *pvParameter)
{
    app_espnow_event_t evt;

    vTaskDelay(500 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    app_espnow_send_param_t *send_param = (app_espnow_send_param_t *)pvParameter;
    if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        app_espnow_deinit(send_param);
        vTaskDelete(NULL);
    }

    while (xQueueReceive(app_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case APP_ESPNOW_SEND_CB:
            {
                // Blink the green led.
                vTaskResume( greenBlinkTaskHandle );

                /* Delay a while before sending the next data. */
                if (send_param->delay > 0) {
                    vTaskDelay(send_param->delay/portTICK_RATE_MS);
                }

                app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                app_espnow_data_prepare(send_param);

                /* Send the next data after the previous data is sent. */
                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                    ESP_LOGE(TAG, "Send error");
                    app_espnow_deinit(send_param);
                    vTaskDelete(NULL);
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
    app_espnow_send_param_t *send_param;

    app_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(app_espnow_event_t));
    if (app_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    if (esp_now_init() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_init");
      abort();
    }

    if (esp_now_register_send_cb(app_espnow_send_cb) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_register_send_cb");
      abort();
    }

    /* Set primary master key. */
    if (esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_set_pmk");
      abort();
    }

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(app_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, app_broadcast_mac, ESP_NOW_ETH_ALEN);
    if (esp_now_add_peer(peer) != ESP_OK) {
      ESP_LOGE(TAG, "Failed: esp_now_add_peer");
      abort();
    }
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(app_espnow_send_param_t));
    memset(send_param, 0, sizeof(app_espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(app_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 0;
    send_param->magic = esp_random();
    send_param->count = CONFIG_ESPNOW_SEND_COUNT;
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(app_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, app_broadcast_mac, ESP_NOW_ETH_ALEN);
    app_espnow_data_prepare(send_param);

    xTaskCreate(app_espnow_task, "app_espnow_task", 2048, send_param, 4, NULL);

    return ESP_OK;
}

static void app_espnow_deinit(app_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(app_espnow_queue);
    esp_now_deinit();
}

void app_main()
{
    denbit_init();
    xTaskCreate(greenBlinkTask, "greenBlinkTask", 180, NULL, 1, &greenBlinkTaskHandle);
    xTaskCreate(redBlinkTask, "redBlinkTask", 180, NULL, 1, &redBlinkTaskHandle);

    // Initialize NVS
    if (nvs_flash_init() != ESP_OK) {
      ESP_LOGE(TAG, "Failed: nvs_flash_init");
      abort();
    }

    app_wifi_init();
    app_espnow_init();
}
