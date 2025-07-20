#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// GPIO pin definitions
#define RELAY               5       // Relay control pin (1:ON/0:OFF)
#define POWER_LED           12      // Power indicator LED (RED) (0:ON/1:OFF)
#define LINK_LED            13      // Network status LED (BLUE) (0:ON/1:OFF)
#define KEY                 0       // Button input pin

// GPIO pin selection masks
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<RELAY) | (1ULL<<POWER_LED) | (1ULL<<LINK_LED))
#define GPIO_INPUT_PIN_SEL  ((1ULL<<KEY))

// 配置结构体 - 包含所有可配置的参数
typedef struct {
    // WiFi配置
    char wifi_ssid[32];
    char wifi_password[64];
    char wifi_encryption[8];  // WPA2, WPA, WEP, NONE
    
    // frp服务器配置
    char frp_server[64];
    uint16_t frp_port;
    char frp_token[64];
    
    // 代理配置
    char proxy_name[32];
    char proxy_type[8];  // tcp, udp, http, https
    char local_ip[16];
    uint16_t local_port;
    uint16_t remote_port;
    
    // 心跳配置
    uint16_t heartbeat_interval;
    uint16_t heartbeat_timeout;
    
    // 配置版本号
    uint32_t config_version;
    
} device_config_t;

// 默认配置
extern const device_config_t default_config;

// 全局配置变量
extern device_config_t g_device_config;

// 配置管理函数
esp_err_t config_init(void);
esp_err_t config_load_from_nvs(void);
esp_err_t config_save_to_nvs(void);
esp_err_t config_reset_to_default(void);
void config_print(void);

bool check_config_mode(void);

#endif // CONFIG_H 