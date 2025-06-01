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

/** @file crypto.c
    @author Copyright (C) 2025 LYC <365256281@qq.com>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/uart.h"
#include <esp_aes.h>
#include <mbedtls/platform.h>
#include <mbedtls/cipher.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <assert.h>
#include <ctype.h>
#include "crypto.h"

/* Global variables */
struct frp_coder *encoder = NULL;  // Encoder structure pointer
struct frp_coder *decoder = NULL;  // Decoder structure pointer

const char *token = "52010";       // Secret token for key derivation
const char *salt = "frp";          // Salt value for PBKDF2

mbedtls_cipher_context_t enc_ctx, dec_ctx; // Encryption/Decryption contexts

/**
 * Initialize decoder structure with given IV 
 * @param iv Initialization vector
 * @return Pointer to initialized decoder structure
 */
struct frp_coder* init_decoder(const uint8_t *iv) {
    const mbedtls_cipher_info_t *cipher_info;
    
    // Allocate and zero-initialize decoder structure
    decoder = calloc(sizeof(struct frp_coder), 1);

    // Copy token and salt into decoder
    decoder->token = strdup(token);
    decoder->salt = strdup(salt);
    
    // Initialize SHA-1 context for PBKDF2
    mbedtls_md_context_t sha1_ctx;
    mbedtls_md_init(&sha1_ctx);

    // Get SHA-1 algorithm information
    const mbedtls_md_info_t *sha1_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);

    // Setup HMAC-based SHA-1 context
    mbedtls_md_setup(&sha1_ctx, sha1_info, 1); // 1 = HMAC mode

    // Derive encryption key using PBKDF2
    mbedtls_pkcs5_pbkdf2_hmac(
        &sha1_ctx,                             // HMAC context
        (const unsigned char *)decoder->token,  // Secret token
        strlen((const char *)decoder->token),   // Token length
        (const unsigned char *)decoder->salt,   // Salt value
        strlen((const char *)decoder->salt),    // Salt length
        64,                                     // Iteration count
        16,                                     // Output key length (16 bytes = 128 bits)
        decoder->key                            // Output key buffer
    );

    // Copy initialization vector
    memcpy(decoder->iv, iv, AES_128_IV_SIZE);
    
    // Initialize AES-128-CFB cipher context
    cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_CFB128);
    mbedtls_cipher_init(&dec_ctx);  
    mbedtls_cipher_setup(&dec_ctx, cipher_info);
    mbedtls_cipher_setkey(&dec_ctx, decoder->key, 128, MBEDTLS_DECRYPT);
    mbedtls_cipher_set_iv(&dec_ctx, decoder->iv, 16);
    
    return decoder;
}

/**
 * Initialize encoder structure with given IV
 * @param iv Initialization vector
 * @return Pointer to initialized encoder structure
 */
struct frp_coder* init_encoder(const uint8_t *iv) {
    const mbedtls_cipher_info_t *cipher_info;
    
    // Allocate and zero-initialize encoder structure
    encoder = calloc(sizeof(struct frp_coder), 1);
    
    // Copy token and salt into encoder
    encoder->token = strdup(token);
    encoder->salt = strdup(salt);
    
    // Initialize SHA-1 context for PBKDF2
    mbedtls_md_context_t sha1_ctx;
    mbedtls_md_init(&sha1_ctx);
    
    // Select the SHA-1 algorithm and initialize
    const mbedtls_md_info_t *sha1_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    
    // Setup HMAC-based SHA-1 context
    mbedtls_md_setup(&sha1_ctx, sha1_info, 1);  // 1 = HMAC mode

    // Derive encryption key using PBKDF2
    mbedtls_pkcs5_pbkdf2_hmac(
        &sha1_ctx,	// HMAC context
        (const unsigned char *)encoder->token,  // Secret token
        strlen((const char *)encoder->token),   // Token length
        (const unsigned char *)encoder->salt,   // Salt value
        strlen((const char *)encoder->salt),    // Salt length
        64,	// Iteration count
        16,	//Output key length (16 bytes = 128 bits)
        encoder->key	// Output key buffer
    );

    // Copy initialization vector
    memcpy(encoder->iv, iv, AES_128_IV_SIZE);
    
    // Initialize AES-128-CFB cipher context
    cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_CFB128);
    mbedtls_cipher_init(&enc_ctx);  
    mbedtls_cipher_setup(&enc_ctx, cipher_info);
    mbedtls_cipher_setkey(&enc_ctx, encoder->key, 128, MBEDTLS_ENCRYPT);
    mbedtls_cipher_set_iv(&enc_ctx, encoder->iv, 16);
    
    return encoder;
}

/**
 * Decrypt data using AES-128-CFB
 * @param ciphertext Pointer to ciphertext buffer
 * @param ct_len Length of ciphertext
 * @param plaintext Pointer to plaintext buffer
 * @param pt_len Pointer to plaintext length (updated by function)
 * @return 0 on success, error code otherwise
 */
int my_aes_decrypt(unsigned char *ciphertext, size_t ct_len, 
                  unsigned char *plaintext, size_t *pt_len) {
    int ret;
    size_t finish_olen;

    // Process ciphertext in multiple steps
    if ((ret = mbedtls_cipher_update(&dec_ctx, ciphertext, ct_len, 
                                    plaintext, pt_len)) != 0) {
        goto exit;
    }

    // Finalize decryption process
    if ((ret = mbedtls_cipher_finish(&dec_ctx, plaintext + *pt_len, 
                                    &finish_olen)) != 0) {
        goto exit;
    }
 
    *pt_len += finish_olen;

exit:
    return ret;  // Return 0 on success, error code otherwise
}

/**
 * Encrypt data using AES-128-CFB
 * @param plaintext Pointer to plaintext buffer
 * @param pt_len Length of plaintext
 * @param ciphertext Pointer to ciphertext buffer
 * @param ct_len Pointer to ciphertext length (updated by function)
 * @return 0 on success, error code otherwise
 */
int my_aes_encrypt(const unsigned char *plaintext, size_t pt_len,
                  unsigned char *ciphertext, size_t *ct_len) {
    int ret;
    size_t finish_olen;

    // Process plaintext in multiple steps
    if ((ret = mbedtls_cipher_update(&enc_ctx, plaintext, pt_len,
                                    ciphertext, ct_len)) != 0) {
        goto exit;
    }

    // Finalize encryption process
    if ((ret = mbedtls_cipher_finish(&enc_ctx, ciphertext + *ct_len,
                                    &finish_olen)) != 0) {
        goto exit;
    }
    
    *ct_len += finish_olen;

exit:
    return ret;  // Return 0 on success, error code otherwise
}
