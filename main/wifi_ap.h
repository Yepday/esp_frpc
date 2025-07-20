#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_err.h"

// WiFi热点管理函数
esp_err_t wifi_ap_init(void);
esp_err_t wifi_ap_start(void);
esp_err_t wifi_ap_stop(void);
const char* wifi_ap_get_ip(void);

// WiFi Station连接函数
esp_err_t wifi_sta_init(void);
esp_err_t wifi_sta_connect(const char* ssid, const char* password);
esp_err_t wifi_sta_disconnect(void);
bool wifi_sta_is_connected(void);

#endif // WIFI_AP_H 