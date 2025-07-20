#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "config.h"
#include "driver/gpio.h"
#include "timer.h"

static const char *TAG = "CONFIG";

// 默认配置
const device_config_t default_config = {
    .wifi_ssid = "ESP8266_Config",
    .wifi_password = "12345678",
    .wifi_encryption = "WPA2",
    .frp_server = "192.168.1.100",
    .frp_port = 7000,
    .frp_token = "52010",
    .proxy_name = "ssh-ubuntu",
    .proxy_type = "tcp",
    .local_ip = "127.0.0.1",
    .local_port = 22,
    .remote_port = 7005,
    .heartbeat_interval = 30,
    .heartbeat_timeout = 90,
    .config_version = 1
};

// 全局配置变量
device_config_t g_device_config;

/**
 * 初始化配置系统
 */
esp_err_t config_init(void)
{
    ESP_LOGI(TAG, "Initializing configuration system...");
    
    // 先加载默认配置
    memcpy(&g_device_config, &default_config, sizeof(device_config_t));
    
    // 尝试从NVS加载配置
    esp_err_t ret = config_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config from NVS, using default config");
        // 保存默认配置到NVS
        //config_save_to_nvs();
    }
    
    config_print();
    return ESP_OK;
}

/**
 * 从NVS加载配置
 */
 // ... existing code ...
esp_err_t config_load_from_nvs(void)
{
    nvs_handle nvs_handle;
    esp_err_t err;

    err = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t version = 0;
    err = nvs_get_u32(nvs_handle, "version", &version);
    if (err != ESP_OK) goto fail;

    if (version != default_config.config_version) {
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_VERSION;
    }

    size_t len;
    len = sizeof(g_device_config.wifi_ssid);
    err = nvs_get_str(nvs_handle, "wifi_ssid", g_device_config.wifi_ssid, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.wifi_password);
    err = nvs_get_str(nvs_handle, "wifi_password", g_device_config.wifi_password, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.wifi_encryption);
    err = nvs_get_str(nvs_handle, "wifi_enc", g_device_config.wifi_encryption, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.frp_server);
    err = nvs_get_str(nvs_handle, "frp_srv", g_device_config.frp_server, &len);
    if (err != ESP_OK) goto fail;
    err = nvs_get_u16(nvs_handle, "frp_port", &g_device_config.frp_port);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.frp_token);
    err = nvs_get_str(nvs_handle, "frp_tok", g_device_config.frp_token, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.proxy_name);
    err = nvs_get_str(nvs_handle, "prx_name", g_device_config.proxy_name, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.proxy_type);
    err = nvs_get_str(nvs_handle, "prx_type", g_device_config.proxy_type, &len);
    if (err != ESP_OK) goto fail;
    len = sizeof(g_device_config.local_ip);
    err = nvs_get_str(nvs_handle, "loc_ip", g_device_config.local_ip, &len);
    if (err != ESP_OK) goto fail;
    err = nvs_get_u16(nvs_handle, "loc_port", &g_device_config.local_port);
    if (err != ESP_OK) goto fail;
    err = nvs_get_u16(nvs_handle, "rmt_port", &g_device_config.remote_port);
    if (err != ESP_OK) goto fail;
    err = nvs_get_u16(nvs_handle, "hb_itvl", &g_device_config.heartbeat_interval);
    if (err != ESP_OK) goto fail;
    err = nvs_get_u16(nvs_handle, "hb_to", &g_device_config.heartbeat_timeout);
    if (err != ESP_OK) goto fail;
    g_device_config.config_version = version;

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Configuration loaded from NVS successfully");
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Error reading config item: %s", esp_err_to_name(err));
    nvs_close(nvs_handle);
    return err;
}
// ... existing code ...
esp_err_t config_save_to_nvs(void)
{
    nvs_handle nvs_handle;
    esp_err_t err;

    err = nvs_open("config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle, "version", g_device_config.config_version);
    if (err != ESP_OK) goto fail;

    err = nvs_set_str(nvs_handle, "wifi_ssid", g_device_config.wifi_ssid);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "wifi_password", g_device_config.wifi_password);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "wifi_enc", g_device_config.wifi_encryption);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "frp_srv", g_device_config.frp_server);
    if (err != ESP_OK) goto fail;
    err = nvs_set_u16(nvs_handle, "frp_port", g_device_config.frp_port);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "frp_tok", g_device_config.frp_token);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "prx_name", g_device_config.proxy_name);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "prx_type", g_device_config.proxy_type);
    if (err != ESP_OK) goto fail;
    err = nvs_set_str(nvs_handle, "loc_ip", g_device_config.local_ip);
    if (err != ESP_OK) goto fail;
    err = nvs_set_u16(nvs_handle, "loc_port", g_device_config.local_port);
    if (err != ESP_OK) goto fail;
    err = nvs_set_u16(nvs_handle, "rmt_port", g_device_config.remote_port);
    if (err != ESP_OK) goto fail;
    err = nvs_set_u16(nvs_handle, "hb_itvl", g_device_config.heartbeat_interval);
    if (err != ESP_OK) goto fail;
    err = nvs_set_u16(nvs_handle, "hb_to", g_device_config.heartbeat_timeout);
    if (err != ESP_OK) goto fail;

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) goto fail;

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Configuration saved to NVS successfully");
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Error writing config item: %s", esp_err_to_name(err));
    nvs_close(nvs_handle);
    return err;
}
// ... existing code ...
/**
 * 重置配置为默认值
 */
esp_err_t config_reset_to_default(void)
{
    ESP_LOGI(TAG, "Resetting configuration to default values...");
    
    memcpy(&g_device_config, &default_config, sizeof(device_config_t));
    
    esp_err_t ret = config_save_to_nvs();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration reset successfully");
    }
    
    return ret;
}

/**
 * 打印当前配置
 */
void config_print(void)
{
    ESP_LOGI(TAG, "=== Current Configuration ===");
    ESP_LOGI(TAG, "WiFi SSID: %s", g_device_config.wifi_ssid);
    ESP_LOGI(TAG, "WiFi Encryption: %s", g_device_config.wifi_encryption);
    ESP_LOGI(TAG, "frp Server: %s:%d", g_device_config.frp_server, g_device_config.frp_port);
    ESP_LOGI(TAG, "Proxy Name: %s", g_device_config.proxy_name);
    ESP_LOGI(TAG, "Proxy Type: %s", g_device_config.proxy_type);
    ESP_LOGI(TAG, "Local Service: %s:%d", g_device_config.local_ip, g_device_config.local_port);
    ESP_LOGI(TAG, "Remote Port: %d", g_device_config.remote_port);
    ESP_LOGI(TAG, "Heartbeat: %d/%d", g_device_config.heartbeat_interval, g_device_config.heartbeat_timeout);
    ESP_LOGI(TAG, "Config Version: %u", g_device_config.config_version);
    ESP_LOGI(TAG, "================================");
} 

/**
 * 检查是否进入配置模式
 * 上电10秒内net灯闪烁（0.5秒切换），检查按钮状态
 */
bool check_config_mode()
{
    ESP_LOGI("MAIN", "Checking for config mode (10 seconds)...");
    uint32_t start_tick = get_tick_count();
    bool led_blink_state = false;
    
    // 10秒 = 100个滴答
    uint32_t relative_tick = 0;
    while (relative_tick < 100) {
        relative_tick = get_tick_count() - start_tick;
        
        // tickcnt == 100 时结束循环
        if (relative_tick >= 100) {
            break;
        }
        
        // NET LED闪烁 - 每5个滴答(0.5秒)切换一次状态
        if (relative_tick % 5 == 0 && relative_tick > 0) {
            led_blink_state = !led_blink_state;
            gpio_set_level(LINK_LED, led_blink_state ? 0 : 1);  // 0=亮，1=灭
        }
        
        // 检查按钮状态（直接读电平）
        if (gpio_get_level(KEY) == 0) {  // 按钮按下（低电平）
            ESP_LOGI("MAIN", "Button pressed! Entering config mode...");
            gpio_set_level(LINK_LED, 0);  // 点亮net灯表示进入配置模式
            return true;
        }
        
        // 短暂延时避免CPU占用过高
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI("MAIN", "No button press detected. Entering normal mode...");
    gpio_set_level(LINK_LED, 1);  // 关闭net灯
    return false;
} 