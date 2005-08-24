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

/* char *pretty_key(char *, size_t, zend_bool, zend_bool) */
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
			decoded_len = len; \
			decoded = estrndup(str, decoded_len); \
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
			return SUCCESS;
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

	return SUCCESS;
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

/* {{{ STATUS http_exit(int, char*, char*, zend_bool) */
STATUS _http_exit_ex(int status, char *header, char *body, zend_bool send_header TSRMLS_DC)
{
	char datetime[128];
	
	if (SUCCESS != http_send_status_header(status, send_header ? header : NULL)) {
		http_error_ex(HE_WARNING, HTTP_E_HEADER, "Failed to exit with status/header: %d - %s", status, header ? header : "");
		STR_FREE(header);
		STR_FREE(body);
		return FAILURE;
	}
	if (body) {
		PHPWRITE(body, strlen(body));
	}
	{
		time_t now;
		struct tm nowtm;
		
		time(&now);
		strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", php_localtime_r(&now, &nowtm));
	}

#define HTTP_LOG_WRITE(for, type, header) \
	HTTP_LOG_WRITE_EX(for, type, header); \
	HTTP_LOG_WRITE_EX(composite, type, header);

#define HTTP_LOG_WRITE_EX(for, type, header) \
	if (HTTP_G(log).##for && strlen(HTTP_G(log).##for)) { \
	 	php_stream *log = php_stream_open_wrapper(HTTP_G(log).##for, "ab", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL); \
		 \
		if (log) { \
			php_stream_printf(log TSRMLS_CC, "%s [%12s] %32s <%s>%s", datetime, type, header, SG(request_info).request_uri, PHP_EOL); \
			php_stream_close(log); \
		} \
	 \
	}
	switch (status)
	{
		case 301:	HTTP_LOG_WRITE(redirect, "301-REDIRECT", header);			break;
		case 302:	HTTP_LOG_WRITE(redirect, "302-REDIRECT", header);			break;
		case 304:	HTTP_LOG_WRITE(cache, "304-CACHE", header);					break;
		case 401:	HTTP_LOG_WRITE(auth, "401-AUTH", header);					break;
		case 403:	HTTP_LOG_WRITE(auth, "403-AUTH", header);					break;
		case 405:	HTTP_LOG_WRITE(allowed_methods, "405-ALLOWED", header);		break;
	}
	STR_FREE(header);
	STR_FREE(body);
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
	zval **hsv;
	zval **var;
	
	if (SUCCESS != zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &hsv)) {
		return NULL;
	}
	if (SUCCESS != zend_hash_find(Z_ARRVAL_PP(hsv), (char *) key, key_size, (void **) &var)) {
		return NULL;
	}
	if (check && !(Z_STRVAL_PP(var) && Z_STRLEN_PP(var))) {
		return NULL;
	}
	return *var;
}
/* }}} */

/* {{{ STATUS http_get_request_body(char **, size_t *) */
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
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Invalid character (expected 0x0D 0x0A; got: 0x%x 0x%x)", *n_ptr, *(n_ptr + 1));
				} else {
					char *error = estrndup(n_ptr, strcspn(n_ptr, "\r\n \0"));
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Invalid chunk size: '%s' at pos %d", error, n_ptr - encoded);
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

