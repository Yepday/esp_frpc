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

/** @file control.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <stdio.h> 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_system.h"
#include "esp_log.h"
#include "control.h"
#include "login.h"
#include "msg.h"
#include "crypto.h"
#include "sntp.h"
#include "tcpmux.h"
#include "timer.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "driver/gpio.h"

// GPIO configuration macros
#define GPIO_OUTPUT_IO_0    12      // GPIO pin 12 for output
#define GPIO_OUTPUT_IO_1    4       // GPIO pin 4 for output
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))  // Bitmask for selected GPIOs

static const char *TAG = "control";

// Global variables
char g_RxBuffer[2048];        // Receive data buffer
char g_IsLogged = 0;          // Login status flag
char g_ProxyWork = 0;         // Proxy service activation flag
uint g_session_id = 1;        // Session ID counter
uint linked = 0;              // Connection established flag

Control_t *g_pMainCtl;        // Main control structure
ProxyService_t *g_pProxyService;
ProxyClient_t *g_pClient;
time_t g_Pongtime = 0;
char client_connected = 0;     // Client connection status

// External declarations
extern struct frp_coder *decoder;
extern login_t *g_pLogin;
extern struct frp_coder *encoder;
extern Control_t *g_pMainCtl;

/**
 * Initialize GPIO pins as outputs with initial states
 */
void init_GPIO() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;        // Disable interrupts
    io_conf.mode = GPIO_MODE_OUTPUT;              // Set as output mode
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;   // Select GPIO pins
    io_conf.pull_down_en = 0;                     // Disable pull-down
    io_conf.pull_up_en = 0;                       // Disable pull-up
    gpio_config(&io_conf);                        // Apply configuration

    gpio_set_level(GPIO_OUTPUT_IO_0, 0);  // Set GPIO12 to LOW
    gpio_set_level(GPIO_OUTPUT_IO_1, 1);  // Set GPIO4 to HIGH
}

/**
 * Establish connection to the remote server
 */
void connect_to_server() {
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    int err;

    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);  // Server IP
    destAddr.sin_family = AF_INET;                      // IPv4
    destAddr.sin_port = htons(SERVER_PORT);             // Server port
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    CreateTimer();  // Initialize timer

    int MainSock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (MainSock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    err = connect(MainSock, (struct sockaddr *)&destAddr, sizeof(destAddr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        close(MainSock);
        RESET_DEVICE;
        return;
    }

    g_pMainCtl->iMainSock = MainSock;
    ESP_LOGI(TAG, "Successfully connected");

    send_window_update(MainSock, &g_pMainCtl->stream, 0);  // window update
    login(MainSock);  // Perform login procedure
    
    while (1) {  // Main processing loop
        process_data();
    }
}

/**
 * Initialize all system components
 */
void initialize() {
    init_GPIO();             // Configure GPIOs
    init_login();            // Initialize login
    init_main_control();     // Setup main control structure
    init_main_config();      // Load main configuration
    init_proxy_Service();    // Configure proxy service
    init_sntp();             // Initialize SNTP for time sync
}

/**
 * Initialize main control structure
 * @return _SUCCESS on success, _FAIL otherwise
 */
int init_main_control() {    
    if (g_pMainCtl && g_pMainCtl->iMainSock) {
        ESP_LOGE(TAG, "error: main control or base socket already exists!");
        close(g_pMainCtl->iMainSock);
        free(g_pMainCtl);
        return _FAIL;
    }
    
    g_pMainCtl = calloc(sizeof(Control_t), 1);
    if (NULL == g_pMainCtl) {
        ESP_LOGE(TAG, "error: main control init failed!");
        return _FAIL;
    }  
    
    g_pMainCtl->iMainSock = -1;
    g_pMainCtl->stream.id = g_session_id;  // Set session ID
    g_pMainCtl->stream.state = INIT;       // Initial stream state

    return _SUCCESS;
}

/**
 * Initialize proxy service configuration
 */
void init_proxy_Service() {
    g_pProxyService = (ProxyService_t *)calloc(sizeof(ProxyService_t), 1);
    if (NULL == g_pProxyService) {
        ESP_LOGE(TAG, "error: init proxy service _FAIL");
        return;
    }

    // Set proxy configuration parameters
    g_pProxyService->proxy_name = strdup(PROXY_NAME);
    g_pProxyService->proxy_type = strdup(PROXY_TYPE);
    g_pProxyService->local_ip = strdup(LOCAL_IP);
    g_pProxyService->local_port = LOCAL_PORT;
    g_pProxyService->remote_port = REMOTE_PORT;

    dump_common_conf();  // Log configuration details
}

/**
 * Perform login procedure
 * @param Sockfd Socket descriptor
 * @return _SUCCESS on success
 */
int login(int Sockfd) {
    char *lg_msg = NULL;
    uint len = login_request_marshal(&lg_msg);  // Marshal login request
    
    if (!lg_msg) {
        ESP_LOGE(TAG, "error: login_request_marshal failed");
        exit(0);
    }
    
    send_msg_frp_server(Sockfd, TypeLogin, lg_msg, len, &g_pMainCtl->stream);
    ESP_LOGI(TAG, "info: end login procedure");
    
    SAFE_FREE(lg_msg);  // Free allocated message buffer
    return _SUCCESS;
}

/**
 * Write data to TCP multiplexing stream
 * @param Sockfd Socket descriptor
 * @param data Data buffer to send
 * @param length Data length
 * @param pstream Stream context
 * @return _SUCCESS on success, _FAIL otherwise
 */
uint tmux_stream_write(int Sockfd, char *data, uint length, tmux_stream_t *pstream) {
    uint uiRet = _SUCCESS;
        
    switch(pstream->state) {
    case LOCAL_CLOSE:
    case CLOSED:
    case RESET:
        ESP_LOGI(TAG, "stream %d state is closed", pstream->id);
        return 0;
    default:
        break;
    }

    ushort flags = get_send_flags(pstream);  // Get protocol flags
    ESP_LOGI(TAG, "tmux_stream_write stream id %u  length %u", pstream->id, length);

    tcp_mux_send_hdr(Sockfd, flags, pstream->id, length);  // Send header
    if (send(Sockfd, data, length, 0) < 0) {               // Send payload
        ESP_LOGE(TAG, "error: tmux_stream_write send FAIL");
        uiRet = _FAIL;
        RESET_DEVICE;
    }   
    return uiRet;
}

/**
 * Start proxy services by sending configuration to server
 */
void start_proxy_services() {
    ProxyService_t *ps = g_pProxyService;
    
    char *new_proxy_msg = NULL;
    int len = new_proxy_service_marshal(ps, &new_proxy_msg);  // Marshal proxy config
    if (!new_proxy_msg) {
        ESP_LOGI(TAG, "proxy service request marshal failed");
        return;
    }

    ESP_LOGI(TAG, "control proxy client: [Type %d : proxy_name %s : msg_len %d]", 
             TypeNewProxy, ps->proxy_name, len);
    
    send_enc_msg_frp_server(g_pMainCtl->iMainSock, TypeNewProxy, new_proxy_msg, len, &g_pMainCtl->stream);
    SAFE_FREE(new_proxy_msg);  // Free message buffer
}

/**
 * Create new proxy client instance
 * @return Initialized proxy client structure
 */
ProxyClient_t *new_proxy_client() {
    g_session_id += 2;  // Increment session ID
    ProxyClient_t *client = calloc(1, sizeof(ProxyClient_t));
    client->stream_id = g_session_id;       // Assign stream ID
    client->iMainSock = g_pMainCtl->iMainSock;  // Share main socket
    client->stream.id = g_session_id;       // Set stream ID
    client->stream.state = INIT;            // Initial stream state
    return client;
}

/**
 * Handle new client connection through TCP multiplexer
 */
void new_client_connect() {
    g_pClient = new_proxy_client();  // Create client instance
    ESP_LOGI(TAG, "new client through tcp mux: %d", g_pClient->stream_id);
    send_window_update(g_pClient->iMainSock, &g_pClient->stream, 0);  // window Update
    new_work_connection(g_pMainCtl->iMainSock, &g_pClient->stream);   // Establish work connection
}

/**
 * Establish new work connection with server
 * @param iSock Main socket descriptor
 * @param stream Stream context
 */
void new_work_connection(int iSock, struct tmux_stream *stream) {
    assert(iSock);
    
    struct work_conn *work_c = calloc(1, sizeof(struct work_conn));
    work_c->run_id = g_pLogin->run_id;  // Get run ID from login context
    if (!work_c->run_id) {
        ESP_LOGI(TAG, "cannot found run ID");
        SAFE_FREE(work_c);
        return;
    }
    
    char *new_work_conn_request_message = NULL;
    int nret = new_work_conn_marshal(work_c, &new_work_conn_request_message);
    if (0 == nret) {
        ESP_LOGI(TAG, "new work connection request marshal failed!");
        return;
    }

    send_msg_frp_server(iSock, TypeNewWorkConn, new_work_conn_request_message, nret, stream);

    SAFE_FREE(new_work_conn_request_message);
    SAFE_FREE(work_c);
}

/**
 * Process incoming data from server
 */
void process_data() {
    struct tcp_mux_header tmux_hdr;
    uint uiHdrLen;
    uint stream_len;
    uint16_t flags;
    int MainSock = g_pMainCtl->iMainSock;
    int streamId;
    int rx_len;
    size_t pt_len;
    struct msg_hdr* mhdr;
    uchar *decrypted = NULL;
    tmux_stream_t cur_stream;
    
    memset(&tmux_hdr, 0, sizeof(tmux_hdr));
    uiHdrLen = read(MainSock, &tmux_hdr, sizeof(tmux_hdr));  // Read header

    if (uiHdrLen < sizeof(tmux_hdr)) {
        ESP_LOGI(TAG, "uiHdrLen [%d] < sizeof tmux_hdr", uiHdrLen);
        RESET_DEVICE;
        return;
    }

    flags = ntohs(tmux_hdr.flags);          // Extract flags
    stream_len = ntohl(tmux_hdr.length);    // Extract data length
    streamId = ntohl(tmux_hdr.stream_id);   // Extract stream ID

    // Select stream context based on ID
    if (1 == streamId) {
        cur_stream = g_pMainCtl->stream;
    } else {
        cur_stream = g_pClient->stream;
    }
    if (!process_flags(flags, &(cur_stream))) {  // Process protocol flags
        return;
    }

    switch (tmux_hdr.type) {
        case DATA: {
            memset(&g_RxBuffer, 0, sizeof(g_RxBuffer));
            rx_len = read(MainSock, g_RxBuffer, stream_len);  // Read payload

            if (0 == rx_len) {  // Connection closed
		close(MainSock);
            	RESET_DEVICE;
		return;
            }

            // Handle encrypted data for main stream
            if (decoder && (1 == streamId)) {
                decrypted = calloc(1, stream_len);
                my_aes_decrypt((uchar*)g_RxBuffer, stream_len, decrypted, &pt_len);
                mhdr = (struct msg_hdr*)decrypted;
                ESP_LOGI(TAG, "type: %c", mhdr->type);
                ESP_LOGI(TAG, "data: %s", mhdr->data);
            } else {  // Plaintext handling
                mhdr = (struct msg_hdr*)g_RxBuffer;
                ESP_LOGI(TAG, "type: %c", mhdr->type);
                ESP_LOGI(TAG, "data: %s", mhdr->data);
            }
            
            if (0 == g_ProxyWork) { 
                if (TypeLoginResp == mhdr->type) {  // Login response
                    ESP_LOGI(TAG, "type: TypeLoginResp");
                    handle_login_response(g_RxBuffer, rx_len);
                    g_IsLogged = 1;
                } else if (g_IsLogged && (NULL == decoder)) {  // Init crypto
                    if (rx_len != 16) {  // Check IV length
                        ESP_LOGI(TAG, "info: iv length != 16");
                        break;
                    }
                    init_decoder((uint8_t*)g_RxBuffer);  // Initialize decoder
                    init_encoder((uint8_t*)g_RxBuffer);  // Initialize encoder
                } else if(TypeReqWorkConn == mhdr->type) {  // Work conn request
                    ESP_LOGI(TAG, "mhdr->type == TypeReqWorkConn");
                    if (!client_connected) {
                        tmux_stream_write(MainSock, (char*)encoder->iv, 16, &g_pMainCtl->stream);
                        start_proxy_services();  // Activate proxy
                        client_connected = 1;
                    }
                    new_client_connect();  // Create client connection
                    g_ProxyWork = 1;       // Enable proxy operation
                } else if(TypeNewProxyResp == mhdr->type) {  // Proxy response
                    ESP_LOGI(TAG, "mhdr->type == TypeNewProxyResp");
                }
            } else {
                if (g_session_id == streamId) {  // Client stream
                    if(1 == linked) {  // Data transfer
                        // Debug print received data
                        printf("\n################DATA RECIEVE################\n");
                        for(int i=0;i<stream_len;i++) {
                            if(isprint(g_RxBuffer[i])) printf("%c", g_RxBuffer[i]); 
                            else printf(".");
                        }
                        printf("\n\n");
                        for(int i=0;i<stream_len;i++) {
                            printf("%02X ", g_RxBuffer[i]);
                        }
                        printf("\n############################################\n\n");
                        
                        // Send acknowledgment
                        char buf[32] = {0};
                        sprintf(buf, "%d bytes recieved!\n", stream_len);
                        tmux_stream_write(g_pMainCtl->iMainSock, buf, strlen(buf), &g_pClient->stream);
                        
                        // GPIO control logic
                        if(strstr(g_RxBuffer, "POWER_ON")) {
                            gpio_set_level(GPIO_OUTPUT_IO_0, 1);  // Activate GPIO12
                            gpio_set_level(GPIO_OUTPUT_IO_1, 0);  // Deactivate GPIO4
                        }
                        if(strstr(g_RxBuffer, "POWER_OFF")) {
                            gpio_set_level(GPIO_OUTPUT_IO_0, 0);  // Deactivate GPIO12
                            gpio_set_level(GPIO_OUTPUT_IO_1, 1);  // Activate GPIO4
                        }
                    }
                    if(TypeStartWorkConn == mhdr->type) linked = 1;  // Mark connection ready
                } else if (1 == streamId) {  // Main control stream
                    ESP_LOGI(TAG, "g_ControlState: StateProxyWork");
                    if (TypePong == mhdr->type) {  // Keep-alive response
                        g_Pongtime = obtain_time();
                        ESP_LOGI(TAG, "msg->type: TypePong");
                    }
                }
                send_window_update(g_pMainCtl->iMainSock, &g_pClient->stream, stream_len);  // Update window
            }
            if (1 == streamId) {
                SAFE_FREE(decrypted);  // Cleanup decryption buffer
            }
            break;
        }
        case PING: {  // Handle keep-alive ping
            handle_tcp_mux_ping(&tmux_hdr);
            break;
        }
    }
}
