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

#include <ctype.h>

#include "php.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_headers_api.h"
#include "php_http_send_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_exception_object.h"
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

/* char *pretty_key(char *, size_t, zend_bool, zebd_bool) */
char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen)
{
	if (key && key_len) {
		unsigned i, wasalpha;
		if (wasalpha = isalpha(key[0])) {
			key[0] = uctitle ? toupper(key[0]) : tolower(key[0]);
		}
		for (i = 1; i < key_len; i++) {
			if (isalpha(key[i])) {
				key[i] = ((!wasalpha) && uctitle) ? toupper(key[i]) : tolower(key[i]);
				wasalpha = 1;
			} else {
				if (xhyphen && (key[i] == '_')) {
					key[i] = '-';
				}
				wasalpha = 0;
			}
		}
	}
	return key;
}
/* }}} */

/* {{{ void http_error(long, long, char*) */
void _http_error_ex(long type, long code, const char *format, ...)
{
	va_list args;
	TSRMLS_FETCH();

	va_start(args, format);
	if (type == E_THROW) {
#ifdef ZEND_ENGINE_2
		char *message;
		vspprintf(&message, 0, format, args);
		zend_throw_exception(http_exception_get_default(), message, code TSRMLS_CC);
#else
		type = E_WARNING;
#endif
	}
	if (type != E_THROW) {
		php_verror(NULL, "", type, format, args TSRMLS_CC);
	}
	va_end(args);
}
/* }}} */

/* {{{ STATUS http_exit(int, char*) */
STATUS _http_exit_ex(int status, char *header, zend_bool free_header TSRMLS_DC)
{
	if (SUCCESS != http_send_status_header(status, header)) {
		http_error_ex(E_WARNING, HTTP_E_HEADER, "Failed to exit with status/header: %d - %s", status, header ? header : "");
		if (free_header && header) {
			efree(header);
		}
		return FAILURE;
	}
	if (free_header && header) {
		efree(header);
	}
	zend_bailout();
	/* fake */
	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_check_method(char *) */
STATUS _http_check_method_ex(const char *method, const char *methods)
{
	const char *found;

	if (	(found = strstr(methods, method)) &&
			(found == method || !isalpha(found[-1])) &&
			(!isalpha(found[strlen(method) + 1]))) {
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ zval *http_get_server_var_ex(char *, size_t) */
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_size, zend_bool check TSRMLS_DC)
{
	zval **var;
	if (SUCCESS == zend_hash_find(HTTP_SERVER_VARS,	(char *) key, key_size, (void **) &var)) {
		if (check) {
			return Z_STRVAL_PP(var) && Z_STRLEN_PP(var) ? *var : NULL;
		} else {
			return *var;
		}
	}
	return NULL;
}
/* }}} */


/* {{{ char *http_chunked_decode(char *, size_t, char **, size_t *) */
PHP_HTTP_API const char *_http_chunked_decode(const char *encoded, size_t encoded_len,
	char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *e_ptr;
	char *d_ptr;

	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len);
	d_ptr = *decoded;
	e_ptr = encoded;

	while (((e_ptr - encoded) - encoded_len) > 0) {
		char hex_len[9] = {0};
		size_t chunk_len = 0;
		int i = 0;

		/* read in chunk size */
		while (isxdigit(*e_ptr)) {
			if (i == 9) {
				http_error_ex(E_WARNING, HTTP_E_PARSE, "Chunk size is too long: 0x%s...", hex_len);
				efree(*decoded);
				return NULL;
			}
			hex_len[i++] = *e_ptr++;
		}

		/* hex to long */
		{
			char *error = NULL;
			chunk_len = strtol(hex_len, &error, 16);
			if (error == hex_len) {
				http_error_ex(E_WARNING, HTTP_E_PARSE, "Invalid chunk size string: '%s'", hex_len);
				efree(*decoded);
				return NULL;
			}
		}

		/* reached the end */
		if (!chunk_len) {
			break;
		}

		/* new line */
		if (strncmp(e_ptr, HTTP_CRLF, 2)) {
			http_error_ex(E_WARNING, HTTP_E_PARSE,
				"Invalid character (expected 0x0D 0x0A; got: 0x%x 0x%x)", *e_ptr, *(e_ptr + 1));
			efree(*decoded);
			return NULL;
		}

		memcpy(d_ptr, e_ptr += 2, chunk_len);
		d_ptr += chunk_len;
		e_ptr += chunk_len + 2;
		*decoded_len += chunk_len;
	}

	return e_ptr;
}
/* }}} */

/* {{{ STATUS http_split_response(zval *, zval *, zval *) */
PHP_HTTP_API STATUS _http_split_response(zval *response, zval *headers, zval *body TSRMLS_DC)
{
	char *b = NULL;
	size_t l = 0;
	STATUS status = http_split_response_ex(Z_STRVAL_P(response), Z_STRLEN_P(response), Z_ARRVAL_P(headers), &b, &l);
	ZVAL_STRINGL(body, b, l, 0);
	return status;
}
/* }}} */

/* {{{ STATUS http_split_response(char *, size_t, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_split_response_ex(char *response, size_t response_len,
	HashTable *headers, char **body, size_t *body_len TSRMLS_DC)
{
	char *header = response, *real_body = NULL;

	while (0 < (response_len - (response - header + 4))) {
		if (	(*response++ == '\r') &&
				(*response++ == '\n') &&
				(*response++ == '\r') &&
				(*response++ == '\n')) {
			real_body = response;
			break;
		}
	}

	if (real_body && (*body_len = (response_len - (real_body - header)))) {
		*body = ecalloc(1, *body_len + 1);
		memcpy(*body, real_body, *body_len);
	}

	return http_parse_headers_ex(header, headers, 1);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

