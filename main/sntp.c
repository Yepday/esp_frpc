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

/** @file sntp.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "sntp.h"
#include "control.h"

static const char *TAG = "sntp_time";

/**
 * Initialize the SNTP (Simple Network Time Protocol) client.
 * Configures the SNTP operating mode to polling and sets the NTP server.
 */
void init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);  // Set SNTP operating mode to periodic polling
    sntp_setservername(0, "ntp.aliyun.com");  // Set primary NTP server address
    sntp_init();  // Initialize SNTP service
}

/**
 * Obtain the current time from the SNTP server.
 * Attempts to retrieve the current time with retries if the time is invalid.
 * 
 * @return The current Unix timestamp on success, -1 on failure.
 */
time_t obtain_time(void)
{
    time_t now = 0;         // Stores current timestamp
    int retry = 0;          // Retry counter
    const int retry_count = 10;  // Maximum retry attempts

    // Loop until valid time is obtained or retry limit reached
    // 1600000000 = September 13, 2020 (arbitrary valid timestamp threshold)
    while (now < 1600000000 && ++retry < retry_count) 
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Delay for 2 seconds (FreeRTOS)
        time(&now);  // Update current time value
    }

    if (now < 1600000000) 
    {
        ESP_LOGE(TAG, "Failed to get SNTP time");
        RESET_DEVICE;
        return -1;  // Return error code
    }

    ESP_LOGI(TAG, "The current time: %ld", now);
    
    return now;  // Return valid Unix timestamp
}
