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

/** @file control.h
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#ifndef CONTROL_H
#define CONTROL_H

#include "tcpmux.h"

#define RESET_DEVICE esp_restart()

// 全局变量声明
extern bool config_mode;  // 配置模式标志（定义在main.c中）

typedef struct Control {
	int                 iMainSock;  	//main socketfd
	tmux_stream_t    	stream;
} Control_t;

typedef enum msg_type {
	TypeLogin                 = 'o',
	TypeLoginResp             = '1',
	TypeNewProxy              = 'p',
	TypeNewProxyResp          = '2',
	TypeCloseProxy            = 'c',
	TypeNewWorkConn           = 'w',
	TypeReqWorkConn           = 'r',
	TypeStartWorkConn         = 's',
	TypeNewVisitorConn        = 'v',
	TypeNewVisitorConnResp    = '3',
	TypePing                  = 'h',
	TypePong                  = '4',
	TypeUDPPacket             = 'u',
	TypeNatHoleVisitor        = 'i',
	TypeNatHoleClient         = 'n',
	TypeNatHoleResp           = 'm',
	TypeNatHoleClientDetectOK = 'd',
	TypeNatHoleSid            = '5',
}msg_type_t;

int  init_main_control();

typedef struct proxy_service {
	char 	*proxy_name;
	char 	*proxy_type;
	char 	*ftp_cfg_proxy_name;
	int 	use_encryption;
	int		use_compression;

	char	*local_ip;
	int		remote_port;
	int 	remote_data_port;
	int 	local_port;
}ProxyService_t;

typedef struct proxy_client {
	int iMainSock;          // xfrpc proxy <---> frps
	int iLocalSock;         // xfrpc proxy <---> local service
	struct tmux_stream 	stream;
	uint32_t				stream_id;
	int						connected;
	int 					work_started;
	struct 	proxy_service 	*ps;
	unsigned char			*data_tail; // storage untreated data
	size_t					data_tail_size;

}ProxyClient_t;


void init_proxy_Service();

void initialize();

int login(int Sockfd);

int send_msg_frp_server(int Sockfd,  //req_msg : type = TypeLogin'o' lenth data
                    			const msg_type_t type, 
                    			const char *pmsg, 
                    			const uint msg_len, 
                    			tmux_stream_t *stream);

uint tmux_stream_write(int Sockfd, char *data, uint length, tmux_stream_t *pstream);  //req_msg : type lenth data

void process_data();

void new_work_connection(int iSock, struct tmux_stream *stream);

void new_client_connect();

void start_proxy_services();

void connect_to_server();

void init_gpio_pins();

#endif
