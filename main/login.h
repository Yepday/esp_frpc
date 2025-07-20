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

/** @file login.h
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#ifndef LOGIN_H
#define LOGIN_H

#define _FAIL 1

#define PROTOCOL_VERESION "0.43.0"

typedef struct login {
	char		*version;
	char		*hostname;
	char 		*os;
	char		*arch;
	char 		*user;
	char 		*privilege_key;
    int 	timestamp;
	char 		*run_id;
	char		*metas;
	int 		pool_count;

	/* fields not need json marshal */
	int			logged;		//0 not login 1:logged
} login_t;

struct login_resp {
	char 	*version;
	char	*run_id;
	char 	*error;
};

typedef struct Main_Conf {
	char	*server_addr; 	/* default 0.0.0.0 */
	int	server_port; 	/* default 7000 */
	char	*auth_token;
	int	heartbeat_interval; /* default 10 */
	int	heartbeat_timeout;	/* default 30 */
} MainConfig_t;

int init_login();
int init_main_config();
void dump_common_conf();
int handle_login_response(const char* buf, int len);
int login_resp_check(struct login_resp* lr);

#endif

