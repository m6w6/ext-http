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

#ifndef PHP_HTTP_API_H
#define PHP_HTTP_API_H

#include "php_http_std_defs.h"
#include "php_http_send_api.h"

#define pretty_key(key, key_len, uctitle, xhyphen) _http_pretty_key(key, key_len, uctitle, xhyphen)
extern char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);

typedef void (*http_key_list_decode_t)(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);
#define http_key_list_default_decoder _http_key_list_default_decoder
extern void _http_key_list_default_decoder(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_parse_cookie(l, i) _http_parse_key_list((l), (i), ';', http_key_list_default_decoder, 1 TSRMLS_CC)
#define http_parse_key_list(l, i, s, d, f) _http_parse_key_list((l), (i), (s), (d), (f) TSRMLS_CC)
extern STATUS _http_parse_key_list(const char *list, HashTable *items, char separator, http_key_list_decode_t decode, zend_bool first_entry_is_name_value_pair TSRMLS_DC);

#define http_error(type, code, string) _http_error_ex(type, code, "%s", string)
#define http_error_ex _http_error_ex
extern void _http_error_ex(long type TSRMLS_DC, long code, const char *format, ...);

#define http_log(f, i, m) _http_log_ex((f), (i), (m) TSRMLS_CC)
extern void http_log_ex(char *file, const char *ident, const char *message TSRMLS_DC);

#define http_exit(s, h) http_exit_ex((s), (h), NULL, 1)
#define http_exit_ex(s, h, b, e) _http_exit_ex((s), (h), (b), (e) TSRMLS_CC)
extern STATUS _http_exit_ex(int status, char *header, char *body, zend_bool send_header TSRMLS_DC);

#define http_check_method(m) http_check_method_ex((m), HTTP_KNOWN_METHODS)
#define http_check_method_ex(m, a) _http_check_method_ex((m), (a))
extern STATUS _http_check_method_ex(const char *method, const char *methods);

#define HTTP_GSC(var, name, ret)  HTTP_GSP(var, name, return ret)
#define HTTP_GSP(var, name, ret) \
		if (!(var = _http_get_server_var_ex(name, strlen(name)+1, 1 TSRMLS_CC))) { \
			ret; \
		}
#define http_got_server_var(v) (NULL != _http_get_server_var_ex((v), sizeof(v), 1 TSRMLS_CC))
#define http_get_server_var(v) http_get_server_var_ex((v), sizeof(v))
#define http_get_server_var_ex(v, s) _http_get_server_var_ex((v), (s), 0 TSRMLS_CC)
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_size, zend_bool check TSRMLS_DC);

#define http_get_request_body(b, l) _http_get_request_body_ex((b), (l), 1 TSRMLS_CC)
#define http_get_Request_body_ex(b, l, d) _http_get_request_body_ex((b), (l), (d) TSRMLS_CC)
PHP_HTTP_API STATUS _http_get_request_body_ex(char **body, size_t *length, zend_bool dup TSRMLS_DC);

#define http_chunked_decode(e, el, d, dl) _http_chunked_decode((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API const char *_http_chunked_decode(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_guess_content_type(mf, mm, d, l, m) _http_guess_content_type((mf), (mm), (d), (l), (m) TSRMLS_CC)
PHP_HTTP_API char *_http_guess_content_type(const char *magic_file, long magic_mode, void *data_ptr, size_t data_len, http_send_mode mode TSRMLS_DC);


#define http_locate_body _http_locate_body
static inline const char *_http_locate_body(const char *message)
{
	const char *cr = strstr(message, "\r\n\r\n");
	const char *lf = strstr(message, "\n\n");

	if (lf && cr) {
		return MIN(lf + 2, cr + 4);
	} else if (lf || cr) {
		return MAX(lf + 2, cr + 4);
	} else {
		return NULL;
	}
}

#define http_locate_eol _http_locate_eol
static inline const char *_http_locate_eol(const char *line, size_t *eol_len)
{
	const char *eol = strpbrk(line, "\r\n");
	
	if (eol_len) {
		*eol_len = eol ? ((eol[0] == '\r' && eol[1] == '\n') ? 2 : 1) : 0;
	}
	return eol;
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

