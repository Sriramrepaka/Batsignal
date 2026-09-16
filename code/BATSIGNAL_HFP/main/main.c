#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_log.h"

#define RELAY_PIN GPIO_NUM_16
#define LED_PIN   GPIO_NUM_23
#define RELAY_TIMEOUT_MS (15 * 60 * 1000) // 15 Minutes in milliseconds

static const char *TAG = "BATSIGNAL";
static volatile bool isConnected = false;
static volatile bool relay_state = false;
static TimerHandle_t relay_timer = NULL;

// Callback triggered when 15-minute timer expires
void relay_timeout_cb(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "15-minute timer expired. Turning Relay OFF.");
    gpio_set_level(RELAY_PIN, 0);
    relay_state = false;
}

void led_task(void *pvParameters) {
    while (1) {
        if (!isConnected) {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(450));
        } else {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void bt_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    if (event == ESP_HF_CLIENT_CONNECTION_STATE_EVT) {
        if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED ||
            param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) {
            ESP_LOGI(TAG, "Phone Connected Successfully!");
            isConnected = true;

            nvs_handle_t nvs_h;
            if (nvs_open("bt_data", NVS_READWRITE, &nvs_h) == ESP_OK) {
                nvs_set_blob(nvs_h, "peer_bda", param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
                nvs_commit(nvs_h);
                nvs_close(nvs_h);
            }
        } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "Phone Disconnected!");
            isConnected = false;
        }
    }

    // Trigger 1: Incoming Phone Calls
    if (event == ESP_HF_CLIENT_CIND_CALL_SETUP_EVT) {
        if (param->call_setup.status == 1) { // Ringing
            ESP_LOGI(TAG, "INCOMING CALL! Triggering Relay ON");
            gpio_set_level(RELAY_PIN, 1);
        } else if (param->call_setup.status == 0) { // Call Ended
            ESP_LOGI(TAG, "Call Ended. Turning Relay OFF");
            gpio_set_level(RELAY_PIN, 0);
            relay_state = false;
            if (relay_timer != NULL) {
                xTimerStop(relay_timer, 0);
            }
        }
    }

    // Trigger 2: Google Assistant (BVRA) Toggle & 15-Minute Timer
    if (event == ESP_HF_CLIENT_BVRA_EVT) {
        ESP_LOGI(TAG, "[BVRA EVENT] Value: %d", param->bvra.value);
        if (param->bvra.value == 1) { // 1 = Voice Recognition Activated
            if (!relay_state) {
                // Turn Relay ON
                relay_state = true;
                gpio_set_level(RELAY_PIN, 1);
                ESP_LOGI(TAG, "*** RELAY TOGGLED ON! (15-minute auto-off timer started) ***");

                // Start 15-minute software timer
                if (relay_timer != NULL) {
                    xTimerStart(relay_timer, 0);
                }
            } else {
                // Turn Relay OFF immediately (Manual Toggle OFF)
                relay_state = false;
                gpio_set_level(RELAY_PIN, 0);
                ESP_LOGI(TAG, "*** RELAY TOGGLED OFF MANUALLY ***");

                // Stop active timer
                if (relay_timer != NULL) {
                    xTimerStop(relay_timer, 0);
                }
            }
        }
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_CFM_REQ_EVT) {
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
    }
}

void app_main(void) {
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0); 

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // Create 15-minute non-blocking software timer
    relay_timer = xTimerCreate("relay_timer", pdMS_TO_TICKS(RELAY_TIMEOUT_MS), pdFALSE, (void *)0, relay_timeout_cb);

    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(esp_bt_gap_cb));
    ESP_ERROR_CHECK(esp_hf_client_register_callback(bt_hf_client_cb));
    ESP_ERROR_CHECK(esp_hf_client_init());

    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t));

    esp_bt_cod_t cod;
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = 0x08;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("BATSIGNAL"));
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));

    // Auto-reconnect to saved Pixel 9
    esp_bd_addr_t peer_bda;
    size_t bda_len = sizeof(esp_bd_addr_t);
    nvs_handle_t nvs_h;
    if (nvs_open("bt_data", NVS_READONLY, &nvs_h) == ESP_OK) {
        if (nvs_get_blob(nvs_h, "peer_bda", peer_bda, &bda_len) == ESP_OK) {
            ESP_LOGI(TAG, "Attempting auto-reconnect to saved phone...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_hf_client_connect(peer_bda);
        }
        nvs_close(nvs_h);
    }

    ESP_LOGI(TAG, "*** BATSIGNAL BLUETOOTH READY ***");
}