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
#include "ext/standard/url.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_headers_api.h"
#include "php_http_send_api.h"

#ifdef ZEND_ENGINE_2
#	include "zend_exceptions.h"
#	include "php_http_exception_object.h"
#endif

#include <ctype.h>

ZEND_EXTERN_MODULE_GLOBALS(http);

/* char *pretty_key(char *, size_t, zend_bool, zebd_bool) */
char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen)
{
	if (key && key_len) {
		size_t i;
		int wasalpha;
		if (wasalpha = isalpha((int) key[0])) {
			key[0] = (char) (uctitle ? toupper((int) key[0]) : tolower((int) key[0]));
		}
		for (i = 1; i < key_len; i++) {
			if (isalpha((int) key[i])) {
				key[i] = (char) (((!wasalpha) && uctitle) ? toupper((int) key[i]) : tolower((int) key[i]));
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

/* {{{ */
void _http_key_list_default_decoder(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	*decoded = estrndup(encoded, encoded_len);
	*decoded_len = (size_t) php_url_decode(*decoded, encoded_len);
}
/* }}} */

/* {{{ */
STATUS _http_parse_key_list(const char *list, HashTable *items, char separator, http_key_list_decode_t decode, zend_bool first_entry_is_name_value_pair TSRMLS_DC)
{
	const char *key = list, *val = NULL;
	int vallen = 0, keylen = 0, done = 0;
	zval array;

	Z_ARRVAL(array) = items;

	if (!(val = strchr(list, '='))) {
		return FAILURE;
	}

#define HTTP_KEYLIST_VAL(array, k, str, len) \
	{ \
		char *decoded; \
		size_t decoded_len; \
		if (decode) { \
			decode(str, len, &decoded, &decoded_len TSRMLS_CC); \
		} else { \
			decoded = estrdup(str); \
			decoded_len = len; \
		} \
		add_assoc_stringl(array, k, decoded, decoded_len, 0); \
	}
#define HTTP_KEYLIST_FIXKEY() \
	{ \
			while (isspace(*key)) ++key; \
			keylen = val - key; \
			while (isspace(key[keylen - 1])) --keylen; \
	}
#define HTTP_KEYLIST_FIXVAL() \
	{ \
			++val; \
			while (isspace(*val)) ++val; \
			vallen = key - val; \
			while (isspace(val[vallen - 1])) --vallen; \
	}

	HTTP_KEYLIST_FIXKEY();

	if (first_entry_is_name_value_pair) {
		HTTP_KEYLIST_VAL(&array, "name", key, keylen);

		/* just one name=value */
		if (!(key = strchr(val, separator))) {
			key = val + strlen(val);
			HTTP_KEYLIST_FIXVAL();
			HTTP_KEYLIST_VAL(&array, "value", val, vallen);
			goto list_done;
		}
		/* additional info appended */
		else {
			HTTP_KEYLIST_FIXVAL();
			HTTP_KEYLIST_VAL(&array, "value", val, vallen);
		}
	}

	do {
		char *keydup = NULL;

		if (!(val = strchr(key, '='))) {
			break;
		}

		/* start at 0 if first_entry_is_name_value_pair==0 */
		if (zend_hash_num_elements(items)) {
			++key;
		}

		HTTP_KEYLIST_FIXKEY();
		keydup = estrndup(key, keylen);
		if (!(key = strchr(val, separator))) {
			done = 1;
			key = val + strlen(val);
		}
		HTTP_KEYLIST_FIXVAL();
		HTTP_KEYLIST_VAL(&array, keydup, val, vallen);
		efree(keydup);
	} while (!done);

list_done:
	return SUCCESS;
}

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
		zend_throw_exception(http_exception_get_for_code(code), message, code TSRMLS_CC);
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

/* {{{ zend_bool http_get_request_body(char **, size_t *) */
PHP_HTTP_API STATUS _http_get_request_body_ex(char **body, size_t *length, zend_bool dup TSRMLS_DC)
{
	*length = 0;
	*body = NULL;

	if (SG(request_info).raw_post_data) {
		*length = SG(request_info).raw_post_data_length;
		*body = (char *) (dup ? estrndup(SG(request_info).raw_post_data, *length) : SG(request_info).raw_post_data);
		return SUCCESS;
	}
	return FAILURE;
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
		int no_crlf = 0;
		char *n_ptr;
		size_t chunk_len = 0;

		chunk_len = strtol(e_ptr, &n_ptr, 16);

		/* check if:
		 * - we could not read in chunk size
		 * - chunk size is not followed by HTTP_CRLF|NUL
		 */
		if ((n_ptr == e_ptr) || (*n_ptr && (no_crlf = strncmp(n_ptr, HTTP_CRLF, lenof(HTTP_CRLF))))) {
			/* don't fail on apperently not encoded data */
			if (e_ptr == encoded) {
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				if (no_crlf) {
					http_error_ex(E_WARNING, HTTP_E_PARSE, "Invalid character (expected 0x0D 0x0A; got: 0x%x 0x%x)", *n_ptr, *(n_ptr + 1));
				} else {
					char *error = estrndup(n_ptr, strcspn(n_ptr, "\r\n \0"));
					http_error_ex(E_WARNING, HTTP_E_PARSE, "Invalid chunk size: '%s' at pos %d", error, n_ptr - encoded);
					efree(error);
				}

				return NULL;
			}
		} else {
			e_ptr = n_ptr;
		}

		/* reached the end */
		if (!chunk_len) {
			break;
		}

		memcpy(d_ptr, e_ptr += 2, chunk_len);
		d_ptr += chunk_len;
		e_ptr += chunk_len + 2;
		*decoded_len += chunk_len;
	}

	return e_ptr;
}
/* }}} */

/* {{{ STATUS http_split_response(char *, size_t, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_split_response(char *response, size_t response_len,
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

