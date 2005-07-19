/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#include "SAPI.h"
#include "ext/standard/base64.h"

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_send_api.h"

/*
 * TODO:
 *  - Digest Auth
 */

/* {{{ STATUS http_auth_basic_header(char*) */
PHP_HTTP_API STATUS _http_auth_basic_header(const char *realm TSRMLS_DC)
{
	char realm_header[1024] = {0};
	snprintf(realm_header, 1023, "WWW-Authenticate: Basic realm=\"%s\"", realm);
	return http_send_status_header(401, realm_header);
}
/* }}} */

/* {{{ STATUS http_auth_credentials(char **, char **) */
PHP_HTTP_API STATUS _http_auth_basic_credentials(char **user, char **pass TSRMLS_DC)
{
	if (strncmp(sapi_module.name, "isapi", 5)) {
		zval *zuser, *zpass;

		HTTP_GSC(zuser, "PHP_AUTH_USER", FAILURE);
		HTTP_GSC(zpass, "PHP_AUTH_PW", FAILURE);

		*user = estrndup(Z_STRVAL_P(zuser), Z_STRLEN_P(zuser));
		*pass = estrndup(Z_STRVAL_P(zpass), Z_STRLEN_P(zpass));

		return SUCCESS;
	} else {
		zval *zauth = NULL;
		HTTP_GSC(zauth, "HTTP_AUTHORIZATION", FAILURE);
		{
			int decoded_len;
			char *colon, *decoded = (char *) php_base64_decode((const unsigned char *) Z_STRVAL_P(zauth), Z_STRLEN_P(zauth), &decoded_len);

			if (colon = strchr(decoded + 6, ':')) {
				*user = estrndup(decoded + 6, colon - decoded - 6);
				*pass = estrndup(colon + 1, decoded + decoded_len - colon - 6 - 1);

				return SUCCESS;
			} else {
				return FAILURE;
			}
		}
	}
}
/* }}} */


/* {{{ Digest * /

#include "ext/standard/php_rand.h"
#include "ext/standard/md5.h"

#define DIGEST_ALGORITHM "MD5"
#define DIGEST_SECRETLEN 20
static unsigned char digest_secret[DIGEST_SECRETLEN];

#define DIGEST_BIN_LEN 16
typedef char http_digest_bin_t[DIGEST_BIN_LEN];
#define DIGEST_HEX_LEN 32
typedef char http_digest_hex_t[DIGEST_HEX_LEN+1];

void _http_auth_global_init(TSRMLS_D)
{
	int i;
	// XX this is pretty loose
	for (i = 0; i < DIGEST_SECRETLEN; ++i) {
		digest_secret[i] = (unsigned char) ((php_rand(TSRMLS_C) % 254) + 1);
	}
}

static void http_digest_line_decoder(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	char l = '\0';
	size_t i;
	phpstr s;

	phpstr_init_ex(&s, encoded_len, 1);

	for (i = 0; i < encoded_len; ++i) {
		if ((encoded[i] != '\\') && (encoded[i] != '"')) {
			phpstr_append(&s, encoded+i, 1);
		}
	}

	phpstr_fix(&s);

	*decoded = PHPSTR_VAL(&s);
	*decoded_len = PHPSTR_LEN(&s);
}

static void http_digest_tohex(http_digest_bin_t bin, http_digest_hex_t hex)
{
	unsigned short i;
	unsigned char j;

	for (i = 0; i < DIGEST_BIN_LEN; i++) {
		j = (bin[i] >> 4) & 0xf;
		if (j <= 9) {
			hex[i*2] = (j + '0');
		} else {
			hex[i*2] = (j + 'a' - 10);
		}
		j = bin[i] & 0xf;
		if (j <= 9) {
			hex[i*2+1] = (j + '0');
		} else {
			hex[i*2+1] = (j + 'a' - 10);
		}
	}
	hex[DIGEST_HEX_LEN] = '\0';
}

static void http_digest_calc_HA1(
	const char *alg,	const char *user,
	const char *realm,	const char *pass,
	const char *nonce,	const char *cnonce,
	http_digest_hex_t HA1)
{
	PHP_MD5_CTX md5;
	http_digest_bin_t HA1_bin;

	PHP_MD5Init(&md5);
	PHP_MD5Update(&md5, user, strlen(user));
	PHP_MD5Update(&md5, ":", 1);
	PHP_MD5Update(&md5, realm, strlen(realm));
	PHP_MD5Update(&md5, ":", 1);
	PHP_MD5Update(&md5, pass, strlen(pass));
	PHP_MD5Final(HA1_bin, &md5);

	if (strcasecmp(alg, "md5-sess") == 0) {
		PHP_MD5Init(&md5);
		PHP_MD5Update(&md5, HA1_bin, DIGEST_BIN_LEN);
		PHP_MD5Update(&md5, ":", 1);
		PHP_MD5Update(&md5, nonce, strlen(nonce));
		PHP_MD5Update(&md5, ":", 1);
		PHP_MD5Update(&md5, cnonce, strlen(cnonce));
		PHP_MD5Final(HA1_bin, &md5);
	}
	http_digest_tohex(HA1_bin, HA1);
}

static void http_digest_calc_response(
	const http_digest_hex_t HA1,
	const char *nonce,	const char *noncecount,
	const char *cnonce,	const char *qop,
	const char *method,	const char *uri,
	http_digest_hex_t ent,
	http_digest_hex_t response)
{
	PHP_MD5_CTX md5;
	http_digest_bin_t HA2;
	http_digest_bin_t bin;
	http_digest_hex_t HA2_hex;

	// calculate H(A2)
	PHP_MD5Init(&md5);
	PHP_MD5Update(&md5, method, strlen(method));
	PHP_MD5Update(&md5, ":", 1);
	PHP_MD5Update(&md5, uri, strlen(uri));
	if (strcasecmp(qop, "auth-int") == 0) {
		PHP_MD5Update(&md5, ":", 1);
		PHP_MD5Update(&md5, HEntity, DIGEST_HEX_LEN);
	}
	PHP_MD5Final(HA2, &md5);
	http_digest_tohex(HA2, HA2_hex);

	// calculate response
	PHP_MD5Init(&md5);
	PHP_MD5Update(&md5, HA1, DIGEST_HEX_LEN);
	PHP_MD5Update(&md5, ":", 1);
	PHP_MD5Update(&md5, nonce, strlen(nonce));
	PHP_MD5Update(&md5, ":", 1);
	if (*qop) {
		PHP_MD5Update(&md5, noncecount, strlen(noncecount));
		PHP_MD5Update(&md5, ":", 1);
		PHP_MD5Update(&md5, cnonce, strlen(cnonce));
		PHP_MD5Update(&md5, ":", 1);
		PHP_MD5Update(&md5, qop, strlen(qop));
		PHP_MD5Update(&md5, ":", 1);
	}
	PHP_MD5Update(&md5, HA2_hex, DIGEST_HEX_LEN);
	PHP_MD5Final(bin, &md5);
	http_digest_tohex(bin, response);
}

PHP_HTTP_API STATUS _http_auth_digest_credentials(HashTable *items TSRMLS_DC)
{
	char *auth;
	zval array, *zauth = NULL;

	HTTP_GSC(zauth, "HTTP_AUTHORIZATION", FAILURE);
	auth = Z_STRVAL_P(zauth);

	if (strncasecmp(auth, "Digest ", sizeof("Digest")) || (!(auth += sizeof("Digest")))) {
		return FAILURE;
	}
	if (SUCCESS != http_parse_key_list(auth, items, ',', http_digest_line_decoder, 0)) {
		return FAILURE;
	}
	if (	!zend_hash_exists(items, "uri", sizeof("uri"))				||
			!zend_hash_exists(items, "realm", sizeof("realm"))			||
			!zend_hash_exists(items, "nonce", sizeof("nonce"))			||
			!zend_hash_exists(items, "username", sizeof("username"))	||
			!zend_hash_exists(items, "response", sizeof("response"))) {
	   zend_hash_clean(items);
	   return FAILURE;
	}
	return SUCCESS;
}

PHP_HTTP_API STATUS _http_auth_digest_header(const char *realm TSRMLS_DC)
{
	return FAILURE;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

