#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NO_FREE_PAGES 1
#define ESP_ERR_NVS_NEW_VERSION_FOUND 2
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
