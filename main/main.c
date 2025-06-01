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
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "control.h"

/**
 * Main application entry point.
 * Initializes system components, network interfaces, and establishes server connection.
 */
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
    
    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop for system events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Connect to WiFi using example configuration
    ESP_ERROR_CHECK(example_connect());

    // Initialize custom hardware components
    initialize();

    // Establish connection with remote server
    connect_to_server();
}
