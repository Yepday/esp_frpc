/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/** @file main.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "config.h"
#include "webserver.h"
#include "wifi_ap.h"
#include "control.h"
#include "driver/gpio.h"
#include "timer.h"  // 添加timer.h以使用get_tick_count函数
// #include "esp_spiffs.h" // 移除SPIFFS头文件

// GPIO pin definitions are now in config.h

/**
 * Main application entry point.
 * Initializes system components, network interfaces, and establishes server connection.
 */
// 全局html数组和长度
uint8_t g_web_html[7200] = {0};  // 7200字节，支持6个1200字节的文件
size_t g_web_html_size = 7200;

// 配置模式标志（全局变量，供其他模块使用）
bool config_mode = false;


// 硬件定时器功能已移至timer.c中的FreeRTOS定时器
// GPIO初始化函数已移至control.c
// 配置模式检查函数已移至config.c

void app_main()
{
    // Configure UART parameters for serial communication
    uart_config_t uart_config = {
        .baud_rate = 115200,         // Set baud rate to 115200
        .data_bits = UART_DATA_8_BITS, // 8-bit data format
        .parity    = UART_PARITY_DISABLE, // No parity check
        .stop_bits = UART_STOP_BITS_1, // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE // No hardware flow control
    };
    // Apply UART configuration to UART0 (console port)
    uart_param_config(UART_NUM_0, &uart_config);

    // Initialize Non-Volatile Storage (NVS)
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize configuration system
    ESP_ERROR_CHECK(config_init());
    
    // Initialize GPIO pins
    init_gpio_pins();
    
    // Initialize timer (needed for config mode detection)
    CreateTimer();
    
    // Check if should enter config mode
    config_mode = check_config_mode();


    
    // 启动时从NVS读取webpart1-6（只读模式）
    nvs_handle nvs_handle_web;
    esp_err_t err_web = nvs_open("config", NVS_READONLY, &nvs_handle_web);
    if (err_web == ESP_OK) {
        size_t part_size = 1200;  // 修改为1200字节，匹配split -b 1200
        size_t len = part_size;
        for (int i = 1; i <= 6; ++i) {  // 读取6个部分
            char key[32];
            snprintf(key, sizeof(key), "webpart%d", i);
            err_web = nvs_get_blob(nvs_handle_web, key, g_web_html + (i-1)*part_size, &len);
            if (err_web != ESP_OK) {
                ESP_LOGE("MAIN", "Failed to read %s from NVS: %s", key, esp_err_to_name(err_web));
            }
            len = part_size;
        }
        nvs_close(nvs_handle_web);
    } else {
        ESP_LOGE("MAIN", "Failed to open NVS for web html: %s", esp_err_to_name(err_web));
    }

    if (config_mode) {
        // 配置模式：启动WiFi热点和Web服务器
        ESP_LOGI("MAIN", "=== Entering Configuration Mode ===");
        ESP_LOGI("MAIN", "Tick count: %u", get_tick_count());
        
        // 进入webserver模式时设置LED状态
        gpio_set_level(LINK_LED, 0);  // 点亮LINK LED表示进入配置模式
        
        // Initialize WiFi Access Point
        ESP_ERROR_CHECK(wifi_ap_init());
        ESP_ERROR_CHECK(wifi_ap_start());

        // Initialize and start Web Server
        ESP_ERROR_CHECK(webserver_init());
        ESP_ERROR_CHECK(webserver_start());
        
        ESP_LOGI("MAIN", "=== ESP8266 Configuration System Ready ===");
        ESP_LOGI("MAIN", "AP SSID: ESP8266_Config (default)");
        ESP_LOGI("MAIN", "AP Password: 12345678 (default)");
        ESP_LOGI("MAIN", "AP IP Address: %s", wifi_ap_get_ip());
        ESP_LOGI("MAIN", "Web Server: http://%s", wifi_ap_get_ip());
        ESP_LOGI("MAIN", "Configure WiFi settings for FRP connection");
        ESP_LOGI("MAIN", "==========================================");

        // 配置模式主循环 - 基于tick计数
        uint32_t last_tick = get_tick_count();
        while (1) {
            // 每秒打印一次状态
            uint32_t current_tick = get_tick_count();
            if ((current_tick - last_tick) >= 10) {  // 10个滴答 = 1秒
                ESP_LOGI("MAIN", "Config mode running... tick: %u", current_tick);
                last_tick = current_tick;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    } else {
        // 正常模式：准备启动frpc客户端
        ESP_LOGI("MAIN", "=== Entering Normal Mode (FRP Client) ===");
        ESP_LOGI("MAIN", "Tick count: %u", get_tick_count());
        ESP_LOGI("MAIN", "Target WiFi SSID: %s", g_device_config.wifi_ssid);
        ESP_LOGI("MAIN", "FRP Server: %s:%d", g_device_config.frp_server, g_device_config.frp_port);
        ESP_LOGI("MAIN", "==========================================");
        
        
        // 初始化TCP/IP网络接口
        ESP_ERROR_CHECK(esp_netif_init());
        
        // 创建默认事件循环
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        // 初始化并连接WiFi Station
        ESP_LOGI("MAIN", "Initializing WiFi Station...");
        ESP_ERROR_CHECK(wifi_sta_init());
        
        ESP_LOGI("MAIN", "Connecting to WiFi: %s", g_device_config.wifi_ssid);
        esp_err_t wifi_ret = wifi_sta_connect(g_device_config.wifi_ssid, g_device_config.wifi_password);
        if (wifi_ret != ESP_OK) {
            ESP_LOGE("MAIN", "Failed to connect to WiFi, entering error state");
            gpio_set_level(LINK_LED, 0);  // 点亮网络LED表示错误
            while(1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
        
        // WiFi连接成功，点亮网络LED
        gpio_set_level(LINK_LED, 0);
        ESP_LOGI("MAIN", "WiFi connected successfully");
        
        // 初始化自定义硬件组件
        ESP_LOGI("MAIN", "Initializing FRP client components...");
        initialize();
        
        // 建立与远程服务器的连接
        ESP_LOGI("MAIN", "Connecting to FRP server...");
        connect_to_server();
    }
}

// FRPC连接状态设置函数现在在timer.c中实现
