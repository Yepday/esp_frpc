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

/** @file crypto.h
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define AES_128_KEY_SIZE 16
#define AES_128_IV_SIZE  16

struct frp_coder {
	uint8_t 	key[16];
	char 		*salt;
	uint8_t 	iv[16];
	char 		*token;
}; 

struct frp_coder* init_decoder(const uint8_t *iv);
struct frp_coder* init_encoder(const uint8_t *iv);

int my_aes_encrypt(const unsigned char *plaintext, size_t pt_len, unsigned char *ciphertext, size_t *ct_len);
int my_aes_decrypt(unsigned char *ciphertext, size_t ct_len, unsigned char *plaintext, size_t *pt_len); 

#endif // _CRYPTO_H_
