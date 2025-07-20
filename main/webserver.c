#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "webserver.h"
#include "config.h"

// 全局html数组声明
extern uint8_t g_web_html[8192];
extern size_t g_web_html_size;

static const char *TAG = "WEBSERVER";

static httpd_handle_t server = NULL;

// HTTP GET处理函数 - 返回配置页面
static esp_err_t get_config_page(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET / - Serving configuration page");
    
    // 设置响应头
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    
    // 直接返回全局html数组内容
    httpd_resp_send(req, (const char*)g_web_html, g_web_html_size);
    
    return ESP_OK;
}

// HTTP POST处理函数 - 处理配置更新
static esp_err_t post_config_update(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /update - Processing configuration update");
    
    // 获取POST数据长度
    size_t content_len = req->content_len;
    if (content_len > 1024) {
        ESP_LOGE(TAG, "POST data too large: %d", content_len);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 分配缓冲区
    char* post_data = malloc(content_len + 1);
    if (!post_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for POST data");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 读取POST数据
    int recv_len = httpd_req_recv(req, post_data, content_len);
    if (recv_len <= 0) {
        ESP_LOGE(TAG, "Failed to receive POST data");
        free(post_data);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    post_data[recv_len] = '\0';
    
    ESP_LOGI(TAG, "Received POST data: %s", post_data);
    
    // 处理配置更新
    esp_err_t ret = handle_config_update(post_data, recv_len);
    free(post_data);
    
    if (ret == ESP_OK) {
        // 返回成功页面
        const char* success_html = 
            "<!DOCTYPE html>\n"
            "<html><head><meta charset=\"UTF-8\"><title>配置更新成功</title></head>\n"
            "<body><h2>配置更新成功！</h2><p>设备将在3秒后重启...</p>\n"
            "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>\n"
            "</body></html>";
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, success_html, strlen(success_html));
        
        // 延迟重启设备
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        // 返回错误页面
        const char* error_html = 
            "<!DOCTYPE html>\n"
            "<html><head><meta charset=\"UTF-8\"><title>配置更新失败</title></head>\n"
            "<body><h2>配置更新失败！</h2><p><a href=\"/\">返回配置页面</a></p></body></html>";
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, error_html, strlen(error_html));
    }
    
    return ESP_OK;
}

// 404处理函数 - 作为通用处理器
static esp_err_t not_found_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "404 - Not Found: %s", req->uri);
    
    const char* not_found_html = 
        "<!DOCTYPE html>\n"
        "<html><head><meta charset=\"UTF-8\"><title>404 - 页面未找到</title></head>\n"
        "<body><h2>404 - 页面未找到</h2><p><a href=\"/\">返回主页</a></p></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, not_found_html, strlen(not_found_html));
    
    return ESP_OK;
}

// URL路由表
static const httpd_uri_t uri_table[] = {
    {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_config_page,
        .user_ctx = NULL
    },
    {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = post_config_update,
        .user_ctx = NULL
    },
    {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = not_found_handler,
        .user_ctx = NULL
    }
};

/**
 * 初始化Web服务器
 */
esp_err_t webserver_init(void)
{
    ESP_LOGI(TAG, "Initializing web server...");
    
    // 配置HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 16;
    
    // 启动HTTP服务器
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 注册URI处理器
    for (int i = 0; i < sizeof(uri_table) / sizeof(uri_table[0]); i++) {
        ret = httpd_register_uri_handler(server, &uri_table[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI handler %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
    }
    
    // ESP8266不支持错误处理器注册，使用通配符路由处理404
    
    ESP_LOGI(TAG, "Web server initialized successfully");
    return ESP_OK;
}

/**
 * 启动Web服务器
 */
esp_err_t webserver_start(void)
{
    if (server == NULL) {
        ESP_LOGE(TAG, "Web server not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Web server is running on port 80");
    return ESP_OK;
}

/**
 * 停止Web服务器
 */
esp_err_t webserver_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    return ESP_OK;
}

/**
 * 解析URL编码的字符串
 */
static char* url_decode(const char* str)
{
    char* decoded = malloc(strlen(str) + 1);
    if (!decoded) return NULL;
    
    char* write_pos = decoded;
    const char* read_pos = str;
    
    while (*read_pos) {
        if (*read_pos == '%' && read_pos[1] && read_pos[2]) {
            char hex[3] = {read_pos[1], read_pos[2], 0};
            *write_pos = strtol(hex, NULL, 16);
            read_pos += 3;
        } else if (*read_pos == '+') {
            *write_pos = ' ';
            read_pos++;
        } else {
            *write_pos = *read_pos;
            read_pos++;
        }
        write_pos++;
    }
    *write_pos = '\0';
    
    return decoded;
}

/**
 * 处理配置更新
 */
esp_err_t handle_config_update(const char* post_data, size_t data_len)
{
    ESP_LOGI(TAG, "Processing configuration update...");
    
    // 解析POST数据（application/x-www-form-urlencoded格式）
    char* data_copy = malloc(data_len + 1);
    if (!data_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for data copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(data_copy, post_data, data_len);
    data_copy[data_len] = '\0';
    
    // 解析表单数据
    char* token = strtok(data_copy, "&");
    while (token) {
        char* equal_sign = strchr(token, '=');
        if (equal_sign) {
            *equal_sign = '\0';
            char* key = url_decode(token);
            char* value = url_decode(equal_sign + 1);
            
            if (key && value) {
                ESP_LOGI(TAG, "Config: %s = %s", key, value);
                
                // 更新配置
                // 只处理非空值，空值保持原配置不变
                if (strcmp(key, "wifi_ssid") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.wifi_ssid, value, sizeof(g_device_config.wifi_ssid) - 1);
                    ESP_LOGI(TAG, "Updated WiFi SSID: %s", value);
                } else if (strcmp(key, "wifi_pass") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.wifi_password, value, sizeof(g_device_config.wifi_password) - 1);
                    ESP_LOGI(TAG, "Updated WiFi password");
                } else if (strcmp(key, "wifi_enc") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.wifi_encryption, value, sizeof(g_device_config.wifi_encryption) - 1);
                    ESP_LOGI(TAG, "Updated WiFi encryption: %s", value);
                } else if (strcmp(key, "frp_server") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.frp_server, value, sizeof(g_device_config.frp_server) - 1);
                    ESP_LOGI(TAG, "Updated FRP server: %s", value);
                } else if (strcmp(key, "frp_port") == 0 && strlen(value) > 0) {
                    g_device_config.frp_port = atoi(value);
                    ESP_LOGI(TAG, "Updated FRP port: %d", g_device_config.frp_port);
                } else if (strcmp(key, "frp_token") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.frp_token, value, sizeof(g_device_config.frp_token) - 1);
                    ESP_LOGI(TAG, "Updated FRP token");
                } else if (strcmp(key, "proxy_name") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.proxy_name, value, sizeof(g_device_config.proxy_name) - 1);
                    ESP_LOGI(TAG, "Updated proxy name: %s", value);
                } else if (strcmp(key, "proxy_type") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.proxy_type, value, sizeof(g_device_config.proxy_type) - 1);
                    ESP_LOGI(TAG, "Updated proxy type: %s", value);
                } else if (strcmp(key, "local_ip") == 0 && strlen(value) > 0) {
                    strncpy(g_device_config.local_ip, value, sizeof(g_device_config.local_ip) - 1);
                    ESP_LOGI(TAG, "Updated local IP: %s", value);
                } else if (strcmp(key, "local_port") == 0 && strlen(value) > 0) {
                    g_device_config.local_port = atoi(value);
                    ESP_LOGI(TAG, "Updated local port: %d", g_device_config.local_port);
                } else if (strcmp(key, "remote_port") == 0 && strlen(value) > 0) {
                    g_device_config.remote_port = atoi(value);
                    ESP_LOGI(TAG, "Updated remote port: %d", g_device_config.remote_port);
                } else if (strcmp(key, "heartbeat_interval") == 0 && strlen(value) > 0) {
                    g_device_config.heartbeat_interval = atoi(value);
                    ESP_LOGI(TAG, "Updated heartbeat interval: %d", g_device_config.heartbeat_interval);
                } else if (strcmp(key, "heartbeat_timeout") == 0 && strlen(value) > 0) {
                    g_device_config.heartbeat_timeout = atoi(value);
                    ESP_LOGI(TAG, "Updated heartbeat timeout: %d", g_device_config.heartbeat_timeout);
                }
            }
            
            free(key);
            free(value);
        }
        token = strtok(NULL, "&");
    }
    
    free(data_copy);
    
    // 保存配置到NVS
    esp_err_t ret = config_save_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration to NVS");
        return ret;
    }
    
    ESP_LOGI(TAG, "Configuration updated successfully");
    config_print();
    
    return ESP_OK;
} 