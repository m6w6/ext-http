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

#ifdef PHP_WIN32
#	define PHP_HTTP_API __declspec(dllexport)
#else
#	define PHP_HTTP_API
#endif

/* make functions that return SUCCESS|FAILURE more obvious */
typedef int STATUS;

/* {{{ enum http_range_status */
typedef enum {
	RANGE_OK,
	RANGE_NO,
	RANGE_ERR
} http_range_status;
/* }}} */

/* {{{ enum http_send_mode */
typedef enum {
	SEND_DATA,
	SEND_RSRC
} http_send_mode;
/* }}} */

char *pretty_key(char *key, int key_len, int uctitle, int xhyphen);

/* {{{ public API */
#define http_is_range_request() zend_hash_exists(HTTP_SERVER_VARS, "HTTP_RANGE", sizeof("HTTP_RANGE"))

#define http_date(t) _http_date((t) TSRMLS_CC)
PHP_HTTP_API char *_http_date(time_t t TSRMLS_DC);

#define http_parse_date(d) _http_parse_date((d))
PHP_HTTP_API time_t _http_parse_date(const char *date);

#define http_send_status(s) sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) (s) TSRMLS_CC)
#define http_send_header(h) http_send_status_header(0, (h))
#define http_send_status_header(s, h) _http_send_status_header((s), (h) TSRMLS_CC)
PHP_HTTP_API inline STATUS _http_send_status_header(const int status, const char *header TSRMLS_DC);

#define http_get_server_var(v) _http_get_server_var((v) TSRMLS_CC)
PHP_HTTP_API inline zval *_http_get_server_var(const char *key TSRMLS_DC);

#define http_etag(p, l, m) _http_etag((p), (l), (m) TSRMLS_CC)
PHP_HTTP_API inline char *_http_etag(const void *data_ptr, const size_t data_len, const http_send_mode data_mode TSRMLS_DC);

#define http_lmod(p, m) _http_lmod((p), (m) TSRMLS_CC)
PHP_HTTP_API inline time_t _http_lmod(const void *data_ptr, const http_send_mode data_mode TSRMLS_DC);

#define http_ob_etaghandler(o, l, ho, hl, m) _http_ob_etaghandler((o), (l), (ho), (hl), (m) TSRMLS_CC)
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC);

#define http_start_ob_handler(f, h, s, e) _http_start_ob_handler((f), (h), (s), (e) TSRMLS_CC)
PHP_HTTP_API STATUS _http_start_ob_handler(php_output_handler_func_t handler_func, char *handler_name, uint chunk_size, zend_bool erase TSRMLS_DC);

#define http_modified_match(entry, modified) _http_modified_match((entry), (modified) TSRMLS_CC)
PHP_HTTP_API int _http_modified_match(const char *entry, const time_t t TSRMLS_DC);

#define http_etag_match(entry, etag) _http_etag_match((entry), (etag) TSRMLS_CC)
PHP_HTTP_API int _http_etag_match(const char *entry, const char *etag TSRMLS_DC);

#define http_send_last_modified(t) _http_send_last_modified((t) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_last_modified(const time_t t TSRMLS_DC);

#define http_send_etag(e, l) _http_send_etag((e), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_etag(const char *etag, const int etag_len TSRMLS_DC);

#define http_send_cache_control(c, l) _http_send_cache_control((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_cache_control(const char *cache_control, const size_t cc_len TSRMLS_DC);

#define http_send_content_type(c, l) _http_send_content_type((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_type(const char *content_type, const size_t ct_len TSRMLS_DC);

#define http_send_content_disposition(f, l, i) _http_send_content_disposition((f), (l), (i) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_disposition(const char *filename, const size_t f_len, const int send_inline TSRMLS_DC);

#define http_cache_last_modified(l, s, cc, ccl) _http_cache_last_modified((l), (s), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_last_modified(const time_t last_modified, const time_t send_modified, const char *cache_control, const size_t cc_len TSRMLS_DC);

#define http_cache_etag(e, el, cc, ccl) _http_cache_etag((e), (el), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, const size_t etag_len, const char *cache_control, const size_t cc_len TSRMLS_DC);

#define http_absolute_uri(url) http_absolute_uri_ex((url), strlen(url), NULL, 0, NULL, 0, 0)
#define http_absolute_uri_ex(url, url_len, proto, proto_len, host, host_len, port) _http_absolute_uri_ex((url), (url_len), (proto), (proto_len), (host), (host_len), (port) TSRMLS_CC)
PHP_HTTP_API char *_http_absolute_uri_ex(const char *url, size_t url_len, const char *proto, size_t proto_len, const char *host, size_t host_len, unsigned port TSRMLS_DC);

#define http_negotiate_language(supported, def) _http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), (def) TSRMLS_CC)
#define http_negotiate_charset(supported, def)  _http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), (def) TSRMLS_CC)
#define http_negotiate_q(e, s, d, t) _http_negotiate_q((e), (s), (d), (t) TSRMLS_CC)
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const zval *supported, const char *def TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, const size_t length TSRMLS_DC);

#define http_send_ranges(r, d, s, m) _http_send_ranges((r), (d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_ranges(HashTable *ranges, const void *data, const size_t size, const http_send_mode mode TSRMLS_DC);

#define http_send_data(d, l) http_send((d), (l), SEND_DATA)
#define http_send(d, s, m) _http_send((d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send(const void *data, const size_t data_size, const http_send_mode mode TSRMLS_DC);

#define http_send_file(f) http_send_stream_ex(php_stream_open_wrapper(f, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL), 1)
#define http_send_stream(s) http_send_stream_ex((s), 0)
#define http_send_stream_ex(s, c) _http_send_stream_ex((s), (c) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *s, zend_bool close_stream TSRMLS_DC);

#define http_chunked_decode(e, el, d, dl) _http_chunked_decode((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded, const size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_urlencode_hash(h, q) _http_urlencode_hash_ex((h), 1, NULL, 0, (q), NULL TSRMLS_CC)
#define http_urlencode_hash_ex(h, o, p, pl, q, ql) _http_urlencode_hash_ex((h), (o), (p), (pl), (q), (ql) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, int override_argsep, char *pre_encoded_data, size_t pre_encoded_len, char **encoded_data, size_t *encoded_len TSRMLS_DC);

#define http_split_response(r, h, b) _http_split_response_ex(Z_STRVAL_P(r), Z_STRLEN_P(r), Z_ARRVAL_P(h), &Z_STRVAL_P(b), &Z_STRLEN_P(b) TSRMLS_CC)
#define http_split_response_ex(r, rl, h, b, bl) _http_split_response_ex((r), (rl), (h), (b), (bl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response_ex(char *response, size_t repsonse_len, HashTable *headers, char **body, size_t *body_len TSRMLS_DC);

#define http_parse_headers(h, l, a) _http_parse_headers((h), (l), (a) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers(char *header, int header_len, HashTable *headers TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers((h) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers(zval *array TSRMLS_DC);

#define http_auth_credentials(u, p) _http_auth_credentials((u), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC);

#define http_auth_header(t, r) _http_auth_header((t), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC);

#ifndef ZEND_ENGINE_2
#define php_url_encode_hash(ht, formstr)	php_url_encode_hash_ex((ht), (formstr), NULL, 0, NULL, 0, NULL, 0, NULL TSRMLS_CC)
PHP_HTTP_API STATUS php_url_encode_hash_ex(HashTable *ht, smart_str *formstr,
				const char *num_prefix, int num_prefix_len,
				const char *key_prefix, int key_prefix_len,
				const char *key_suffix, int key_suffix_len,
				zval *type TSRMLS_DC);
#endif

/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

