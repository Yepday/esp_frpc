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

/** @file timer.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>
#include "msg.h"
#include "sntp.h"
#include "timer.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

TimerHandle_t FrpcTimer;             // Handle for the FRPC timer
extern time_t g_Pongtime;
extern Control_t *g_pMainCtl;        // External main control structure pointer
static const char *TAG = "timer";

// 定时器相关变量
static volatile uint32_t tickcnt = 0;     // 滴答计数器，每0.1秒递增
static uint32_t ping_tick_count = 0;      // Ping计数器
static bool led_state = false;            // LED状态

// 连接状态管理
typedef enum {
    CONNECTION_DISCONNECTED,    // 未连接或其他状态 - LED不亮
    CONNECTION_CONNECTED,       // 连接成功 - LED常亮
    CONNECTION_LOST            // 连接丢失 - LED快速闪烁
} connection_state_t;

static connection_state_t frpc_connection_state = CONNECTION_DISCONNECTED;

/**
 * Timer callback function definition
 * This function is called every 0.1 seconds.
 */
void TimerCallback(TimerHandle_t xTimer) 
{
    tickcnt++;  // 滴答计数器递增
    ping_tick_count++;  // Ping计数器递增

    // NET LED控制逻辑（非webserver模式下）
    if (!config_mode) {
        switch (frpc_connection_state) {
            case CONNECTION_DISCONNECTED:
                // 未连接状态 - LED不亮
                gpio_set_level(LINK_LED, 1);  // 关闭LED
                break;
                
            case CONNECTION_CONNECTED:
                // 连接成功 - LED常亮
                gpio_set_level(LINK_LED, 0);  // 点亮LED
                break;
                
            case CONNECTION_LOST:
                // 断线状态 - 快速闪烁（每2个滴答=0.2秒切换一次）
                if (tickcnt % 2 == 0) {
                    led_state = !led_state;
                    gpio_set_level(LINK_LED, led_state ? 0 : 1);
                }
                break;
        }
    }

    // Ping逻辑 - 每300个tick（30秒）执行一次
    if (ping_tick_count >= 300) {
        ping_tick_count = 0;  // 重置ping计数器
        
        // 检查是否有有效的控制结构和socket
        if (g_pMainCtl && g_pMainCtl->iMainSock > 0) {
            // 获取当前时间
            time_t current_time = obtain_time();
            
            // 计算距离上次pong的时间间隔
            int interval = current_time - g_Pongtime;
            
            // 检查是否超时（40秒阈值）
            if (g_Pongtime && interval > 40) {
                ESP_LOGI(TAG, "Time out");
                // 超时会导致后续的RESET_DEVICE，不需要LED闪烁
                return;
            }
            
            // 发送周期性ping到FRPS服务器
            ESP_LOGI(TAG, "ping frps");
            char *ping_msg = "{}";
            send_enc_msg_frp_server(
                g_pMainCtl->iMainSock,      // Main socket descriptor
                TypePing,                   // Message type: PING
                ping_msg,                   // Empty JSON payload
                strlen(ping_msg),           // Message length
                &g_pMainCtl->stream        // Stream context
            );
        }
    }

    // 滴答计数器清零 - 防止溢出
    if (tickcnt == 1000) {
        tickcnt = 0;
    }
}

/**
 * Function to create and start the timer
 * Initializes and starts the timer with 0.1 second period for LED control and periodic pings.
 */
void CreateTimer() 
{
    const char* timerName = "FrpcTimer";                 // Timer identifier
    const TickType_t timerPeriod = pdMS_TO_TICKS(100);   // Convert 100ms to ticks (0.1 second)
    const UBaseType_t autoReload = pdTRUE;               // Enable auto-reload mode
    
    // Create software timer
    FrpcTimer = xTimerCreate(
        timerName,       // Timer name for debugging
        timerPeriod,      // Timer period (0.1 seconds)
        autoReload,       // Auto-reload when expired
        (void*)0,         // No parameters passed to callback
        TimerCallback     // Callback function pointer
    );

    // Check timer creation result
    if (NULL == FrpcTimer) {
        ESP_LOGI(TAG, "FrpcTimer create fail!");
    } else {
        ESP_LOGI(TAG, "FrpcTimer create success!");
        
        // Attempt to start the timer
        if (xTimerStart(FrpcTimer, 0) != pdPASS) {  // 0 = don't wait if queue is full
            ESP_LOGI(TAG, "FrpcTimer start fail!");
        } else {
            ESP_LOGI(TAG, "FrpcTimer start success!");
        }
    }
}

/**
 * Set FRPC connection state to connected
 */
void set_frpc_connection_connected(void) 
{
    frpc_connection_state = CONNECTION_CONNECTED;
    ESP_LOGI(TAG, "FRPC connection established - NET LED on");
}

/**
 * Set FRPC connection state to lost
 */
void set_frpc_connection_lost(void) 
{
    frpc_connection_state = CONNECTION_LOST;
    ESP_LOGI(TAG, "FRPC connection lost - NET LED fast blink");
}

/**
 * Set FRPC connection state to disconnected
 */
void set_frpc_connection_disconnected(void) 
{
    frpc_connection_state = CONNECTION_DISCONNECTED;
    ESP_LOGI(TAG, "FRPC disconnected - NET LED off");
}

/**
 * Get current tick count
 */
uint32_t get_tick_count(void) 
{
    return tickcnt;
}
