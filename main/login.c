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

/** @file login.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_log.h"
#include "login.h"
#include "msg.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

login_t *g_pLogin;         // Global login information structure
MainConfig_t *g_pMainConf; // Global main configuration structure

static const char *TAG = "login";

/**
 * Initialize the login structure with default values and device information.
 * 
 * This function allocates memory for the login structure, sets default values
 * for various fields, retrieves the device MAC address, and uses it to generate
 * a unique run identifier.
 * 
 * @return _SUCCESS on success, _FAIL on failure (e.g., memory allocation error).
 */
int init_login()
{
    char mac_str[13]; // Buffer for MAC address string
    
    // Allocate memory for login structure
    g_pLogin = calloc(sizeof(login_t), 1);
    if (NULL == g_pLogin)
    {
        ESP_LOGE(TAG, "Calloc login_t _FAIL\r\n");
        return _FAIL;
    }

    // Initialize login structure fields
    g_pLogin->version        = strdup(PROTOCOL_VERESION); // Protocol version from define
    g_pLogin->hostname       = NULL; 
    g_pLogin->os             = strdup("freeRTOS");       // Operating system
    g_pLogin->arch           = strdup("Xtensa");          // CPU architecture
    g_pLogin->user           = NULL;
    g_pLogin->timestamp      = 0;                         // Initialize timestamp
    g_pLogin->run_id         = NULL;
    g_pLogin->metas          = NULL;
    g_pLogin->pool_count     = 1;
    g_pLogin->privilege_key  = NULL;
    g_pLogin->logged         = 0;

    uint8_t mac[6]; // Buffer for MAC address
    
    // Read WiFi station MAC address
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    if (ESP_OK == ret) 
    {
        ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } 
    else 
    {
        ESP_LOGE(TAG, "Failed to read MAC\n");
    }

    // Format MAC address as run_id 
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Set run_id as MAC address string
    g_pLogin->run_id = strdup((char*)mac_str);

    // Log initialization results
    ESP_LOGI(TAG, "info: version = %s  run_id = %s", 
                    g_pLogin->version,  g_pLogin->run_id);

    return _SUCCESS;
}

/**
 * Initialize the main configuration structure with server and communication parameters.
 * 
 * This function allocates memory for the main configuration structure and sets
 * default values for server address, port, authentication token, and heartbeat parameters.
 * 
 * @return _SUCCESS on success, _FAIL on failure (e.g., memory allocation error).
 */
int init_main_config()
{
    g_pMainConf = (MainConfig_t *)calloc(sizeof(MainConfig_t), 1);
    if (NULL == g_pMainConf)
    {
        ESP_LOGE(TAG, "error: init main config _FAIL\r\n");
        return _FAIL;
    }

    // Set configuration values from defines
    g_pMainConf->server_addr = strdup(SERVER_ADDR);              // Server address
    g_pMainConf->server_port = SERVER_PORT;                      // Server port
    g_pMainConf->auth_token = strdup(AUTH_TOKEN);                // Authentication token
    g_pMainConf->heartbeat_interval = HEARTBEAT_INTERVAL;        // Heartbeat interval
    g_pMainConf->heartbeat_timeout = HEARTBEAT_TIMEOUT;          // Heartbeat timeout

    dump_common_conf(); // Print configuration details

    return _SUCCESS;
}

/**
 * Print the main configuration parameters for debugging purposes.
 * 
 * This function logs the current configuration settings including server address,
 * port, authentication token, heartbeat interval, and timeout.
 */
void dump_common_conf()
{
    if(! g_pMainConf) {
        ESP_LOGE(TAG, "Error: g_pMainConf is NULL");
        return;
    }

    // Log configuration parameters
    ESP_LOGI(TAG, "Main control {server_addr:%s, server_port:%d, auth_token:%s, interval:%d, timeout:%d}",
             g_pMainConf->server_addr, g_pMainConf->server_port, g_pMainConf->auth_token,
             g_pMainConf->heartbeat_interval, g_pMainConf->heartbeat_timeout);
}

/**
 * Process the login response from the server.
 * 
 * This function parses the login response message, checks its validity,
 * and handles any additional data following the login response.
 * 
 * @param buf Pointer to the received message buffer.
 * @param len Length of the received message buffer.
 * @return _SUCCESS if login is successful, _FAIL otherwise.
 */
int handle_login_response(const char* buf, int len) 
{
    msg_hdr_t* mhdr = (struct msg_hdr*)buf; // Parse message header
    ESP_LOGI(TAG, "handle_login_response");

    // Unmarshal login response data
    struct login_resp* lres = login_resp_unmarshal((const char*)mhdr->data);
    if (!lres) {
        return _FAIL;
    }

    // Validate login response
    if (!login_resp_check(lres)) {
        ESP_LOGI(TAG, "login failed");
        free(lres);
        return _FAIL;
    }
    free(lres);

    // Calculate remaining data length
    int login_len = ntohs(mhdr->length); // Convert network byte order to host
    int lien = len - login_len - sizeof(struct msg_hdr);

    ESP_LOGI(TAG, "login success! login_len %d len %d lien %d", login_len, len, lien);

    if (lien < 0) {
        ESP_LOGI(TAG, "handle_login_response: lien < 0");
        return _FAIL;
    }
    return _SUCCESS;
}

/**
 * Validate the login response from the server.
 * 
 * This function checks if the login response contains valid information
 * such as a valid run ID. If valid, it updates the global login structure
 * with the received information and sets the login status to success.
 * 
 * @param lr Pointer to the login response structure.
 * @return 1 if login response is valid, 0 otherwise.
 */
int login_resp_check(struct login_resp* lr) 
{
    // Check if run_id is valid
    if ((NULL == lr->run_id) || strlen(lr->run_id) <= 1) {
        if (lr->error && strlen(lr->error) > 0) {
            ESP_LOGI(TAG, "login response error: %s", lr->error);
        }
        ESP_LOGI(TAG, "login failed!");
        g_pLogin->logged = 0; // Update login status
        return 0;
    }

    // Update login information with server response
    ESP_LOGI(TAG, "login response: run_id: [%s], version: [%s]",
          lr->run_id, lr->version);
    SAFE_FREE(g_pLogin->run_id);           // Free existing run_id
    g_pLogin->run_id = strdup(lr->run_id); // Set new run_id
    g_pLogin->logged = 1;                 // Set logged-in status

    return 1;
}
