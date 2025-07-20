#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_err.h"

// Web服务器管理函数
esp_err_t webserver_init(void);
esp_err_t webserver_start(void);
esp_err_t webserver_stop(void);

// 配置更新处理函数
esp_err_t handle_config_update(const char* post_data, size_t data_len);

#endif // WEBSERVER_H 