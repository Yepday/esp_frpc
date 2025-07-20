#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "tcpip_adapter.h"
#include "wifi_ap.h"
#include "config.h"

static const char *TAG = "WIFI_AP";

static char ap_ip[16] = "192.168.4.1";

// WiFi事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP started successfully");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "WiFi AP stopped");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/**
 * 初始化WiFi热点
 */
esp_err_t wifi_ap_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Access Point...");
    
    // 初始化TCP/IP适配器
    tcpip_adapter_init();
    
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));
    
    // 设置WiFi模式为AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    ESP_LOGI(TAG, "WiFi AP initialized successfully");
    return ESP_OK;
}

/**
 * 启动WiFi热点
 */
esp_err_t wifi_ap_start(void)
{
    ESP_LOGI(TAG, "Starting WiFi Access Point...");
    
    // 配置AP参数
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = 1,
            .beacon_interval = 100,
        },
    };
    
    // 使用默认的SSID和密码（不使用用户配置）
    strncpy((char*)wifi_config.ap.ssid, "ESP8266_Config", sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, "12345678", sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    
    // 设置认证模式为WPA2（固定）
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    
    // 设置WiFi配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 等待WiFi启动完成
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // 停止DHCP服务
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    
    // 配置AP的IP地址
    tcpip_adapter_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_err_t ret = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set AP IP info: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 重新启动DHCP服务
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
    
    ESP_LOGI(TAG, "WiFi AP started with SSID: %s", wifi_config.ap.ssid);
    ESP_LOGI(TAG, "AP IP address: %s", ap_ip);
    
    return ESP_OK;
}

/**
 * 停止WiFi热点
 */
esp_err_t wifi_ap_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi Access Point...");
    
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    ESP_LOGI(TAG, "WiFi AP stopped");
    return ESP_OK;
}

/**
 * 获取AP的IP地址
 */
const char* wifi_ap_get_ip(void)
{
    return ap_ip;
}

// ============= WiFi Station 功能实现 =============

static bool sta_connected = false;

// WiFi Station事件处理函数
static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi Station started, attempting to connect...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi Station disconnected, attempting to reconnect...");
        sta_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi Station got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        sta_connected = true;
    }
}

/**
 * 初始化WiFi Station模式
 */
esp_err_t wifi_sta_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Station...");
    
    // 初始化TCP/IP适配器
    tcpip_adapter_init();
    
    // 创建默认事件循环（如果还没有创建）
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_sta_event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_sta_event_handler,
                                               NULL));
    
    // 设置WiFi模式为Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    ESP_LOGI(TAG, "WiFi Station initialized successfully");
    return ESP_OK;
}

/**
 * 连接到WiFi网络
 */
esp_err_t wifi_sta_connect(const char* ssid, const char* password)
{
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);
    
    wifi_config_t wifi_config = {0};
    
    // 设置SSID和密码
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    // 设置WiFi配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 等待连接建立（最多等待10秒）
    int retry_count = 0;
    while (!sta_connected && retry_count < 100) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        retry_count++;
    }
    
    if (sta_connected) {
        ESP_LOGI(TAG, "Successfully connected to WiFi");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi within timeout");
        return ESP_FAIL;
    }
}

/**
 * 断开WiFi连接
 */
esp_err_t wifi_sta_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting from WiFi...");
    sta_connected = false;
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_LOGI(TAG, "WiFi Station disconnected");
    return ESP_OK;
}

/**
 * 检查WiFi是否已连接
 */
bool wifi_sta_is_connected(void)
{
    return sta_connected;
} 