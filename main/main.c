/*
 * Demo OTA - ESP32-S3 SuperMini
 *
 * - El LED RGB (WS2812 en GPIO48) titila con el color de ESTA versión.
 * - Al presionar el botón BOOT (GPIO0) la placa descarga OTA_URL por HTTPS y se reinicia.
 *   El servidor se valida contra el certificado embebido (main/certs/server_cert.pem).
 *   Durante la descarga el LED queda en amarillo; si falla, parpadea en rojo.
 *
 * v1.0.0 -> azul   |   v2.0.0 -> verde (cambia FW_R/G/B y PROJECT_VER)
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_desc.h"
#include "driver/gpio.h"
#include "led_strip.h"

/* ======================= CONFIGURACIÓN ======================= */
#define WIFI_SSID   "ESP32-OTA-DEMO"
#define WIFI_PASS   "12345678"
#define OTA_URL     "https://192.168.1.0:8070/esp32s3_ota_demo.bin"  // IP de tu PC

#define LED_GPIO    48   // WS2812 de la SuperMini (si no enciende, prueba otro pin de tu variante)
#define BOOT_GPIO   0

/* Color de esta versión (brillo bajo: el WS2812 es muy luminoso) */
#define FW_R  40        // v2: rojo
#define FW_G  0
#define FW_B  0         // v1: azul
/* ============================================================= */

static const char *TAG = "ota_demo";

/* Certificado del servidor, embebido desde main/certs/server_cert.pem */
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_server_cert_pem_end");

static led_strip_handle_t s_led;
static EventGroupHandle_t s_wifi_events;
#define WIFI_CONNECTED_BIT BIT0

/* ------------------------------ LED ------------------------------ */
static void led_init(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
    led_strip_clear(s_led);
}

static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(s_led, 0, r, g, b);
    led_strip_refresh(s_led);
}

/* ------------------------------ Wi-Fi ------------------------------ */
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    s_wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ------------------------------ OTA ------------------------------ */
static void ota_run(void)
{
    ESP_LOGI(TAG, "Iniciando OTA desde %s", OTA_URL);
    led_set(40, 25, 0);  // amarillo = descargando

    esp_http_client_config_t http_cfg = {
        .url = OTA_URL,
        .cert_pem = (const char *)server_cert_pem_start,  // único cert de confianza (pinning)
        .skip_cert_common_name_check = true,  // el cert es autofirmado y fijo; así no depende de la IP
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA OK, reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    ESP_LOGE(TAG, "OTA falló: %s", esp_err_to_name(err));
    for (int i = 0; i < 6; i++) {  // rojo parpadeando = error
        led_set((i & 1) ? 0 : 40, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ------------------------------ main ------------------------------ */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "Firmware versión %s (compilado %s %s)", app->version, app->date, app->time);

    led_init();
    wifi_init();

    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    bool on = false;
    int tick = 0;

    while (1) {
        if (gpio_get_level(BOOT_GPIO) == 0) {  // BOOT presionado
            if (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) {
                ota_run();
            } else {
                ESP_LOGW(TAG, "Sin Wi-Fi todavía, no se puede hacer OTA");
            }
            while (gpio_get_level(BOOT_GPIO) == 0) {  // esperar a que lo suelte
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }

        if (++tick >= 10) {  // 10 x 50 ms = 500 ms
            tick = 0;
            on = !on;
            if (on) led_set(FW_R, FW_G, FW_B); else led_set(0, 0, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
