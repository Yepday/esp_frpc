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

/** @file tcpmux.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "control.h"
#include "tcpmux.h"

extern Control_t *g_pMainCtl;       // Main control structure
extern char g_ProxyWork;
extern int linked;                  // Connection status flag

// 连接状态控制函数声明 (定义在timer.c中)
extern void set_frpc_connection_lost(void);

static char proto_version = 0;      // Protocol version number
static const char *TAG = "tcpmux";

/**
 * Get the flags to be sent based on the current state of the stream.
 * This function determines the appropriate flags (e.g., SYN, ACK) to send
 * based on the current state of the stream and updates the stream's state.
 * 
 * @param pStream Pointer to the stream structure
 * @return Flags to be sent in the TCP multiplexing header
 */
ushort get_send_flags(struct tmux_stream *pStream)
{
    ushort flags = 0;
    ESP_LOGI(TAG, "get_send_flags: state %d, stream_id %d", pStream->state, pStream->id);
    
    switch (pStream->state) 
    {
    case INIT:
        flags |= SYN;                // Set SYN flag for initial state
        pStream->state = SYN_SEND;   // Transition to SYN_SEND state
        break;            
    case SYN_RECEIVED:
        flags |= ACK;                // Set ACK flag after receiving SYN
        pStream->state = ESTABLISHED; // Transition to ESTABLISHED state
        break;
    default:
        break;
    } 

    return flags;
}

/**
 * Encode TCP multiplexing header fields into a structured format.
 * This function populates the TCP multiplexing header with the provided
 * type, flags, stream ID, and length information, ensuring proper byte ordering.
 * 
 * @param type Type of the TCP multiplexing message
 * @param flags Control flags for the message
 * @param uiStreamId Stream identifier
 * @param uiLength Length of the payload
 * @param ptmux_hdr Pointer to the header structure to be filled
 */
void tcp_mux_encode(tcp_mux_type_t type, tcp_mux_flag_t flags, uint uiStreamId, uint uiLength, tcp_mux_header_t *ptmux_hdr)
{
    if (NULL == ptmux_hdr)
    {
        ESP_LOGE(TAG, "ptmux_hdr is NULL\r\n");
    }   
    // Set header fields with proper byte ordering
    ptmux_hdr->version = proto_version;
    ptmux_hdr->type = type;
    ptmux_hdr->flags = htons(flags);
    ptmux_hdr->stream_id = htonl(uiStreamId);
    ptmux_hdr->length = htonl(uiLength);
    
    ESP_LOGI(TAG, "info: ptmux_hdr version = %u, type = %u, flags = %u, stream_id = %u, length = %u",
             proto_version, type, flags, uiStreamId, uiLength);
}

/**
 * Send a window update message to adjust the receive window size.
 * This function constructs and sends a window update message with the
 * specified length, including appropriate flags based on the stream state.
 * 
 * @param iSockfd Socket file descriptor
 * @param pStream Pointer to the stream structure
 * @param uiLength Length of the window update
 * @return Success or failure code
 */
int send_window_update(int iSockfd, tmux_stream_t *pStream, uint uiLength)
{    
    ushort flags = get_send_flags(pStream);    
    tcp_mux_header_t tmux_hdr;
    uint uiStreamId = pStream->id;

    memset(&tmux_hdr, 0, sizeof(tmux_hdr));
    tcp_mux_encode(WINDOW_UPDATE, flags, uiStreamId, uiLength, &tmux_hdr);
    
    // Send the encoded header
    if(send(iSockfd, (uchar *)&tmux_hdr, sizeof(tmux_hdr), 0)<0) 
    {
        ESP_LOGE(TAG, "error: window update send FAIL");
        RESET_DEVICE;
    }
    
    ESP_LOGI(TAG, "send window update: flags %d, stream_id %d, length %u", flags, pStream->id, uiLength);

    return _SUCCESS;
}

/**
 * Send a TCP multiplexing header for data transmission.
 * This function constructs and sends a data header with the specified
 * flags, stream ID, and payload length.
 * 
 * @param iSockfd Socket file descriptor
 * @param flags Control flags for the data message
 * @param stream_id Stream identifier
 * @param length Length of the data payload
 */
void tcp_mux_send_hdr(int iSockfd, ushort flags, uint stream_id, uint length)
{
    struct tcp_mux_header tmux_hdr;
    
    memset(&tmux_hdr, 0, sizeof(tmux_hdr));
    tcp_mux_encode(DATA, flags, stream_id, length, &tmux_hdr);
    if(send(iSockfd, (char *)&tmux_hdr, sizeof(tmux_hdr), 0)<0) 
    {
        ESP_LOGE(TAG, "error: tcp mux hdr send_FAIL");
        RESET_DEVICE;
    }
    ESP_LOGI(TAG, "tcp mux header,stream_id:%d len:%d", stream_id, length);
}

/**
 * Process control flags received in a TCP multiplexing header.
 * This function handles different flags (ACK, FIN, RST) and updates
 * the stream state accordingly, managing connection state transitions.
 * 
 * @param flags Control flags from the header
 * @param stream Pointer to the stream structure
 * @return Non-zero on success, zero on error
 */
int process_flags(uint16_t flags, struct tmux_stream *stream)
{
    uint32_t close_stream = 0;
    if (ACK == (flags & ACK)) {
        // Handle ACK flag
        if (SYN_SEND == stream->state) stream->state = ESTABLISHED;
    } else if (FIN == (flags & FIN)) {
        // Handle FIN flag (connection termination)
        g_ProxyWork = 0;
        linked = 0;
        set_frpc_connection_lost();  // 正常断开时设置LED闪烁
        
        switch(stream->state) {
        case SYN_SEND:
        case SYN_RECEIVED:
        case ESTABLISHED:
            stream->state = REMOTE_CLOSE;
            break;
        case LOCAL_CLOSE:
            stream->state = CLOSED;
            close_stream = 1;
            break;
        default:
            ESP_LOGI(TAG, "unexpected FIN flag in state %d", stream->state);
            assert(0);
            return 0;
        }
    } else if (RST == (flags & RST)) {
        // Handle RST flag (connection reset)
        stream->state = RESET;
        close_stream = 1;
    }

    if (close_stream) {
        ESP_LOGI(TAG, "free stream %d", stream->id);        
    }

    return 1;
}

/**
 * Handle a TCP multiplexing ping request.
 * This function processes ping requests and sends back appropriate responses
 * when a SYN flag is detected in the ping message.
 * 
 * @param pTmux_hdr Pointer to the TCP multiplexing header of the ping message
 */
void handle_tcp_mux_ping(struct tcp_mux_header *pTmux_hdr)
{    
    uint16_t flags = ntohs(pTmux_hdr->flags);
    uint32_t ping_id = ntohl(pTmux_hdr->length);

    if (SYN == (flags & SYN)) 
    {        
        struct tcp_mux_header Tmux_hdr_send = {0};

        // Prepare and send ping response
        tcp_mux_encode(PING, ACK, 0, ping_id, &Tmux_hdr_send);
        
        ESP_LOGI(TAG, "PING");        
        
        if (send(g_pMainCtl->iMainSock, &Tmux_hdr_send, sizeof(Tmux_hdr_send), 0) < 0)
        {
            ESP_LOGI(TAG, "error: handle tcp mux ping send FAIL");
            RESET_DEVICE;
            return;
        }        
    }
}
