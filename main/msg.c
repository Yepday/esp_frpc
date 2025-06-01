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

/** @file msg.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <esp_system.h>
#include <esp8266/esp_md5.h>
#include "esp_system.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "login.h"
#include "control.h"
#include "msg.h"
#include "sntp.h"
#include "crypto.h"

static const char *TAG = "msg";

extern login_t *g_pLogin;
extern MainConfig_t *g_pMainConf;

/**
 * @brief Send plain text message to FRP server
 * @param Sockfd: Socket file descriptor for communication
 * @param type: Message type (defined in msg_type_t enum)
 * @param pmsg: Pointer to message payload data
 * @param msg_len: Length of message payload
 * @param stream: Pointer to tmux stream structure for I/O operations
 * @return _SUCCESS(1)/_FAIL(0) on operation result
 */
int send_msg_frp_server(int Sockfd, 
                     const msg_type_t type, 
                     const char *pmsg, 
                     const uint msg_len, 
                     tmux_stream_t *stream)
{
    if (Sockfd < 0) {
        ESP_LOGE(TAG, "error: send_msg_frp_server failed, Sockfd < 0");
        return 0;
    }
    ESP_LOGE(TAG, "send plain msg ----> [%c: %s]", type, pmsg);
    
    // Allocate memory for message header + payload
    size_t len = msg_len + sizeof(msg_hdr_t);
    msg_hdr_t *req_msg = calloc(len, 1);
    
    if (NULL == req_msg) {
        ESP_LOGE(TAG, "error: req_msg init failed");
        return _FAIL;
    }   
    
    // Construct message header
    req_msg->type = type;
    req_msg->length = ntoh64((uint64_t)msg_len);  // Convert to network byte order
    memcpy(req_msg->data, pmsg, msg_len);          // Copy payload
    
    // Send through TMUX stream
    tmux_stream_write(Sockfd, (char *)req_msg, len, stream);
    free(req_msg);

    return _SUCCESS;
}

/**
 * @brief Send encrypted message to FRP server
 * @param Sockfd: Socket file descriptor for communication
 * @param type: Message type (from msg_type enum)
 * @param msg: Pointer to message payload data
 * @param msg_len: Length of message payload
 * @param stream: Pointer to tmux stream structure for I/O operations
 */
void send_enc_msg_frp_server(int Sockfd,
             const enum msg_type type, 
             const char *msg, 
             const size_t msg_len, 
             struct tmux_stream *stream)
{
    size_t ct_len;
    
    if (Sockfd < 0) {
        ESP_LOGE(TAG, "error: send_msg_frp_server failed, Sockfd < 0");
        return;
    }

    // Create message header + payload
    struct msg_hdr *req_msg = calloc(msg_len+sizeof(struct msg_hdr), 1);
    assert(req_msg);
    req_msg->type = type;
    req_msg->length = ntoh64((uint64_t)msg_len);
    memcpy(req_msg->data, msg, msg_len);

    // Encrypt the entire message
    uint8_t *enc_msg = calloc(msg_len+sizeof(struct msg_hdr), 1);
    my_aes_encrypt((uint8_t *)req_msg, msg_len+sizeof(struct msg_hdr), enc_msg, &ct_len);

    // Send encrypted data
    tmux_stream_write(Sockfd, (char*)enc_msg, ct_len, stream);  

    // Cleanup resources
    free(enc_msg);    
    free(req_msg);
}

/**
 * Generate authentication key using MD5 hash
 * @param token Authentication token (can be NULL)
 * @param timestamp Output parameter for current timestamp
 * @return MD5 hash string (needs to be freed by caller)
 */
char * get_auth_key(const char *token, int *timestamp)
{
    char seed[128] = {0};
    *timestamp = obtain_time();  // Get current timestamp
    
    // Create seed string: token + timestamp or just timestamp
    if (token)
        snprintf(seed, 128, "%s%d", token, *timestamp);
    else
        snprintf(seed, 128, "%d", *timestamp);
    
    return calc_md5(seed, strlen(seed));  // Calculate MD5 of seed
}

/**
 * Marshal login request to JSON format
 * @param msg Output parameter for generated JSON string
 * @return Length of generated JSON string (0 on failure)
 */
uint32_t login_request_marshal(char **msg) 
{
    uint32_t nret = 0;
    uint32_t freeHeap;
    *msg = NULL;

    // Free existing privilege key
    SAFE_FREE(g_pLogin->privilege_key);
    
    // Generate new authentication key
    char *auth_key = get_auth_key(g_pMainConf->auth_token, &g_pLogin->timestamp);
    if (!auth_key) {
        ESP_LOGE(TAG, "get_auth_key fail");
        return 0;
    }
    
    // Store new privilege key
    g_pLogin->privilege_key = strdup(auth_key);
    if (!g_pLogin->privilege_key) {
        ESP_LOGE(TAG, "privilege_key fail");
        SAFE_FREE(auth_key);
        return 0;
    }

    // Create JSON object for login request
    cJSON *j_login_req = cJSON_CreateObject();
    if (!j_login_req) {
        ESP_LOGE(TAG, "cJSON_CreateObject fail");
        SAFE_FREE(auth_key);
        return 0;
    }
    
    // Add all required fields to JSON
    freeHeap = esp_get_free_heap_size();
    ESP_LOGE(TAG, "Free Heap: %u bytes\n", freeHeap);
    
    cJSON_AddStringToObject(j_login_req, "version", SAFE_JSON_STRING(g_pLogin->version));
    cJSON_AddStringToObject(j_login_req, "hostname", SAFE_JSON_STRING(g_pLogin->hostname));
    cJSON_AddStringToObject(j_login_req, "os", SAFE_JSON_STRING(g_pLogin->os));
    cJSON_AddStringToObject(j_login_req, "arch", SAFE_JSON_STRING(g_pLogin->arch));
    cJSON_AddStringToObject(j_login_req, "user", SAFE_JSON_STRING(g_pLogin->user));
    cJSON_AddStringToObject(j_login_req, "privilege_key", SAFE_JSON_STRING(g_pLogin->privilege_key));
    
    char buf[32] = {0};
    sprintf(buf , "%d" , g_pLogin->timestamp);
    cJSON_AddRawToObject(j_login_req, "timestamp", buf);
    cJSON_AddStringToObject(j_login_req, "run_id", SAFE_JSON_STRING(g_pLogin->run_id));
    sprintf(buf , "%d" , g_pLogin->pool_count);
    cJSON_AddRawToObject(j_login_req, "pool_count", buf);
    cJSON_AddNullToObject(j_login_req, "metas");
    
    // Generate JSON string
    char *tmp = cJSON_PrintUnformatted(j_login_req);
    ESP_LOGE(TAG, "tmp = %p\n", tmp);
    if (tmp && strlen(tmp) > 0) {
        nret = strlen(tmp);
        *msg = strdup(tmp);
        if (!*msg) {
            nret = 0;
        }
    }

    // Cleanup resources
    if (tmp) {
        cJSON_free(tmp);
    }
    cJSON_Delete(j_login_req);
    SAFE_FREE(auth_key);
    return nret;
}

/**
 * Calculate MD5 hash of input data
 * @param data Input data buffer
 * @param datalen Length of input data
 * @return MD5 hash string (33-byte buffer with null terminator)
 */
char * calc_md5(const char *data, int datalen) {
    unsigned char digest[16] = {0};
    char *out = malloc(33);
    if (NULL == out) {
        return NULL;
    }

    // Calculate MD5 hash
    struct MD5Context md5;
    esp_md5_init(&md5);
    esp_md5_update(&md5, (const uint8_t *)data, datalen);
    esp_md5_final(&md5, digest);
    
    // Convert to hex string
    for (int n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 3, "%02x", (unsigned int)digest[n]);
    }
    out[32] = '\0';

    return out;
}

/**
 * @brief Unmarshal login response JSON into structure
 * @param jres: Pointer to JSON response string
 * @return Pointer to login_resp structure (needs free()), NULL on failure
 */
struct login_resp* login_resp_unmarshal(const char* jres) 
{
    	struct login_resp* lr = (struct login_resp*)calloc(1, sizeof(struct login_resp));
    	if (NULL == lr) {
        	return NULL;
    	}

    	cJSON* j_lg_res = cJSON_Parse(jres);
    	if (NULL == j_lg_res) {
        	free(lr);
        	return NULL;
    	}

    	cJSON* l_version = cJSON_GetObjectItem(j_lg_res, "version");
    	if (!cJSON_IsString(l_version)) {
        	goto END_ERROR;
    	}
    	lr->version = strdup(l_version->valuestring);

    	cJSON* l_run_id = cJSON_GetObjectItem(j_lg_res, "run_id");
    	if (!cJSON_IsString(l_run_id)) {
        	goto END_ERROR;
    	}
    	lr->run_id = strdup(l_run_id->valuestring);
    	cJSON_Delete(j_lg_res);
    	return lr;

END_ERROR:
    	cJSON_Delete(j_lg_res);
    	free(lr);
    	return NULL;
}

/**
 * @brief Marshal new proxy service configuration into JSON
 * @param np_req: Pointer to proxy service configuration structure
 * @param msg: Pointer to store generated JSON string (needs free())
 * @return 1 on success, 0 on failure
 */
int new_proxy_service_marshal(const struct proxy_service *np_req, char **msg)
{
    	char *tmp = NULL;
    	int nret = 0;
    
    	if (!np_req || !msg) {
        	//printf("Error: Invalid input\n");
        	return 0;
    	}
    
     	cJSON *j_np_req = cJSON_CreateObject();
    	if (!j_np_req) {
        	//printf("\r\ncJSON_CreateObject fail\r\n");
        	return 0;
    	}
    
    	if (np_req->proxy_name) {
        	//printf("Add proxy_name: %s\n", np_req->proxy_name);
        	cJSON_AddStringToObject(j_np_req, "proxy_name", np_req->proxy_name);
    	} else {
        	cJSON_AddNullToObject(j_np_req, "proxy_name");
    	}
    
    	if (np_req->proxy_type) {
        	//printf("Add proxy_type: %s\n", np_req->proxy_type);
        	cJSON_AddStringToObject(j_np_req, "proxy_type", np_req->proxy_type);
    	} else {
        	cJSON_AddNullToObject(j_np_req, "proxy_type");
    	}

    	cJSON_AddBoolToObject(j_np_req, "use_encryption", np_req->use_encryption);
    	cJSON_AddBoolToObject(j_np_req, "use_compression", np_req->use_compression);
    
    	char buffer[32];
    	if (np_req->remote_port != -1) {
        	snprintf(buffer, sizeof(buffer), "%d", np_req->remote_port);
        	cJSON_AddRawToObject(j_np_req, "remote_port", buffer);
        	//printf("Add remote_port: %d\n", np_req->remote_port);
    	} else {
        	cJSON_AddNullToObject(j_np_req, "remote_port");
    	}
    
    	tmp = cJSON_Print(j_np_req);

    	if (!tmp) {
        	//printf("Error: Failed to print JSON (out of memory)\n");
        	cJSON_Delete(j_np_req);
        	return 0;
    	}

    	if (tmp && strlen(tmp) > 0) {
        	nret = strlen(tmp);
        	*msg = (char *)malloc(nret + 1);
        	if (*msg) {
            		strcpy(*msg, tmp);
        	} else {
            		nret = 0;
        	}
    	}
    
    	free((void *)tmp);
    	cJSON_Delete(j_np_req);
    
    	return nret;
}

/**
 * @brief Marshal new work connection information into JSON
 * @param work_c: Pointer to work connection structure
 * @param msg: Pointer to store generated JSON string (needs free())
 * @return Length of JSON string on success, 0 on failure
 */
int new_work_conn_marshal(const struct work_conn *work_c, char **msg) 
{
    	int nret = 0;
    	cJSON *j_new_work_conn = cJSON_CreateObject();
    	if (!j_new_work_conn) {
        	return 0;
    	}

    	// Add the run_id to the JSON object
    	cJSON_AddStringToObject(j_new_work_conn, "run_id", work_c->run_id ? work_c->run_id : "");

    	// Convert the cJSON object to a JSON string
    	char *tmp = cJSON_PrintUnformatted(j_new_work_conn);
    	if (tmp && strlen(tmp) > 0) {
        	nret = strlen(tmp);
        	*msg = strdup(tmp);
        	assert(*msg);
    	}

    	// Clean up
    	cJSON_Delete(j_new_work_conn);
    	free(tmp);

    	return nret;
}

/**
 * @brief Convert 64-bit value from network byte order to host byte order
 * @param input: 64-bit value in network byte order (big-endian)
 * @return Converted value in host byte order (little-endian on ESP8266)
 */
uint64_t ntoh64(const uint64_t input)
{
    	uint64_t rval;
    	uint8_t *data = (uint8_t *)&rval;

    	data[0] = input >> 56;
    	data[1] = input >> 48;
    	data[2] = input >> 40;
    	data[3] = input >> 32;
    	data[4] = input >> 24;
    	data[5] = input >> 16;
    	data[6] = input >> 8;
    	data[7] = input >> 0;

    	return rval;
}

