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

/** @file tcpmux.h
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#ifndef TCPMUX_H
#define TCPMUX_H

#define _SUCCESS 0
typedef unsigned char uchar;

enum tcp_mux_state {
    INIT = 0,
    SYN_SEND,
    SYN_RECEIVED,
    ESTABLISHED,
    LOCAL_CLOSE,
    REMOTE_CLOSE,
    CLOSED,
    RESET
};

typedef enum tcp_mux_flag {
    ZERO,
    SYN,
    ACK = 1<<1,
    FIN = 1<<2,
    RST = 1<<3,
}tcp_mux_flag_t;

typedef enum tcp_mux_type {
    DATA,
    WINDOW_UPDATE,
    PING,
    GO_AWAY,
}tcp_mux_type_t;

typedef struct __attribute__((__packed__)) tcp_mux_header
{
    uchar    version;
    uchar    type;
    ushort   flags;
    uint     stream_id;
    uint     length;
}tcp_mux_header_t;


typedef struct tmux_stream {
    uint    id;
    enum tcp_mux_state state;   

}tmux_stream_t;

ushort get_send_flags(struct tmux_stream *pStream);

void  tcp_mux_encode(tcp_mux_type_t type, tcp_mux_flag_t flags, uint uiStreamId, uint uiLength, tcp_mux_header_t *ptmux_hdr);

int send_window_update(int iSockfd, tmux_stream_t *pStream, uint uiLength);

void tcp_mux_send_hdr(int iSockfd, ushort flags, uint stream_id, uint length);

int process_flags(uint16_t flags, struct tmux_stream *stream);

void handle_tcp_mux_ping(struct tcp_mux_header *pTmux_hdr);

#endif
