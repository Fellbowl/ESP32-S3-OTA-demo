idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash
                  esp_http_client esp_https_ota esp_app_format
                  esp_driver_gpio bootloader_support app_update
)