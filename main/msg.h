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

/** @file msg.h
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#ifndef MSG_H
#define MSG_H

#include "control.h"

#define SAFE_JSON_STRING(str) (str ? str : "")
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while (0)

typedef struct __attribute__((__packed__)) msg_hdr {
	char		type;
	uint64_t	length;
	char		data[];
} msg_hdr_t;

struct work_conn {
	char *run_id;
};


char * calc_md5(const char *data, int datalen);

char * get_auth_key(const char *token, int *timestamp);

uint login_request_marshal(char **msg);

struct login_resp* login_resp_unmarshal(const char *jres);

int new_proxy_service_marshal(const struct proxy_service *np_req, char **msg);

int send_msg_frp_server(int Sockfd,  //req_msg : type = TypeLogin'o' lenth data
                    			const msg_type_t type, 
                    			const char *pmsg, 
                    			const uint msg_len, 
                    			tmux_stream_t *stream);


void send_enc_msg_frp_server(int Sockfd,
			 const enum msg_type type, 
			 const char *msg, 
			 const size_t msg_len, 
			 struct tmux_stream *stream);

int new_work_conn_marshal(const struct work_conn *work_c, char **msg);

uint64_t ntoh64(const uint64_t input);

#endif
