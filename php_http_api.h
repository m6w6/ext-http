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

char *pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);

/* {{{ public API */
#define http_date(t) _http_date((t) TSRMLS_CC)
PHP_HTTP_API char *_http_date(time_t t TSRMLS_DC);

#define http_parse_date(d) _http_parse_date((d))
PHP_HTTP_API time_t _http_parse_date(const char *date);

#define http_send_status(s) sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) (s) TSRMLS_CC)
#define http_send_header(h) http_send_status_header(0, (h))
#define http_send_status_header(s, h) _http_send_status_header((s), (h) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_status_header(int status, const char *header TSRMLS_DC);

#define http_got_server_var(v) (NULL != _http_get_server_var_ex((v), sizeof(v), 1 TSRMLS_CC))
#define http_get_server_var(v) http_get_server_var_ex((v), sizeof(v))
#define http_get_server_var_ex(v, s) _http_get_server_var_ex((v), (s), 0 TSRMLS_CC)
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_size, zend_bool check TSRMLS_DC);

#define http_etag(p, l, m) _http_etag((p), (l), (m) TSRMLS_CC)
PHP_HTTP_API char *_http_etag(const void *data_ptr, size_t data_len, http_send_mode data_mode TSRMLS_DC);

#define http_lmod(p, m) _http_lmod((p), (m) TSRMLS_CC)
PHP_HTTP_API time_t _http_lmod(const void *data_ptr, http_send_mode data_mode TSRMLS_DC);

#define http_ob_etaghandler(o, l, ho, hl, m) _http_ob_etaghandler((o), (l), (ho), (hl), (m) TSRMLS_CC)
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC);

#define http_start_ob_handler(f, h, s, e) _http_start_ob_handler((f), (h), (s), (e) TSRMLS_CC)
PHP_HTTP_API STATUS _http_start_ob_handler(php_output_handler_func_t handler_func, char *handler_name, uint chunk_size, zend_bool erase TSRMLS_DC);

#define http_modified_match(entry, modified) _http_modified_match_ex((entry), (modified), 1 TSRMLS_CC)
#define http_modified_match_ex(entry, modified, ep) _http_modified_match_ex((entry), (modified), (ep) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_modified_match_ex(const char *entry, time_t t, zend_bool enforce_presence TSRMLS_DC);

#define http_etag_match(entry, etag) _http_etag_match_ex((entry), (etag), 1 TSRMLS_CC)
#define http_etag_match_ex(entry, etag, ep) _http_etag_match_ex((entry), (etag), (ep) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_etag_match_ex(const char *entry, const char *etag, zend_bool enforce_presence TSRMLS_DC);

#define http_send_last_modified(t) _http_send_last_modified((t) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_last_modified(time_t t TSRMLS_DC);

#define http_send_etag(e, l) _http_send_etag((e), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_etag(const char *etag, size_t etag_len TSRMLS_DC);

#define http_send_cache_control(c, l) _http_send_cache_control((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_cache_control(const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_send_content_type(c, l) _http_send_content_type((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_type(const char *content_type, size_t ct_len TSRMLS_DC);

#define http_send_content_disposition(f, l, i) _http_send_content_disposition((f), (l), (i) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_disposition(const char *filename, size_t f_len, zend_bool send_inline TSRMLS_DC);

#define http_cache_last_modified(l, s, cc, ccl) _http_cache_last_modified((l), (s), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_last_modified(time_t last_modified, time_t send_modified, const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_cache_etag(e, el, cc, ccl) _http_cache_etag((e), (el), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, size_t etag_len, const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_absolute_uri(url) http_absolute_uri_ex((url), strlen(url), NULL, 0, NULL, 0, 0)
#define http_absolute_uri_ex(url, url_len, proto, proto_len, host, host_len, port) _http_absolute_uri_ex((url), (url_len), (proto), (proto_len), (host), (host_len), (port) TSRMLS_CC)
PHP_HTTP_API char *_http_absolute_uri_ex(const char *url, size_t url_len, const char *proto, size_t proto_len, const char *host, size_t host_len, unsigned port TSRMLS_DC);

#define http_negotiate_language(zsupported, def) http_negotiate_language_ex(Z_ARRVAL_P(zsupported), (def))
#define http_negotiate_language_ex(supported, def) http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), (def))
#define http_negotiate_charset(zsupported, def) http_negotiate_charset_ex(Z_ARRVAL_P(zsupported), (def))
#define http_negotiate_charset_ex(supported, def)  http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), (def))
#define http_negotiate_q(e, s, d) _http_negotiate_q((e), (s), (d) TSRMLS_CC)
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const HashTable *supported, const char *def TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC);

#define http_send_ranges(r, d, s, m) _http_send_ranges((r), (d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_ranges(HashTable *ranges, const void *data, size_t size, http_send_mode mode TSRMLS_DC);

#define http_send_data(d, l) http_send((d), (l), SEND_DATA)
#define http_send(d, s, m) _http_send((d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send(const void *data, size_t data_size, http_send_mode mode TSRMLS_DC);

#define http_send_file(f) http_send_stream_ex(php_stream_open_wrapper(f, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL), 1)
#define http_send_stream(s) http_send_stream_ex((s), 0)
#define http_send_stream_ex(s, c) _http_send_stream_ex((s), (c) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *s, zend_bool close_stream TSRMLS_DC);

#define http_chunked_decode(e, el, d, dl) _http_chunked_decode((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_urlencode_hash(h, q) _http_urlencode_hash_ex((h), 1, NULL, 0, (q), NULL TSRMLS_CC)
#define http_urlencode_hash_ex(h, o, p, pl, q, ql) _http_urlencode_hash_ex((h), (o), (p), (pl), (q), (ql) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, zend_bool override_argsep, char *pre_encoded_data, size_t pre_encoded_len, char **encoded_data, size_t *encoded_len TSRMLS_DC);

#define http_split_response(r, h, b) _http_split_response((r), (h), (b) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response(zval *response, zval *headers, zval *body TSRMLS_DC);
#define http_split_response_ex(r, rl, h, b, bl) _http_split_response_ex((r), (rl), (h), (b), (bl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response_ex(char *response, size_t repsonse_len, HashTable *headers, char **body, size_t *body_len TSRMLS_DC);

#define http_parse_headers(h, l, a) _http_parse_headers_ex((h), (l), Z_ARRVAL_P(a), 1 TSRMLS_CC)
#define http_parse_headers_ex(h, l, ht, p) _http_parse_headers_ex((h), (l), (ht), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers_ex(char *header, size_t header_len, HashTable *headers, zend_bool prettify TSRMLS_DC);

#define http_parse_cookie(c, ht) _http_parse_cookie((c), (ht) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_cookie(const char *cookie, HashTable *values TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers_ex(Z_ARRVAL_P(h), 1 TSRMLS_CC)
#define http_get_request_headers_ex(h, p) _http_get_request_headers_ex((h), (s) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC);

#define http_auth_credentials(u, p) _http_auth_credentials((u), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC);

#define http_auth_header(t, r) _http_auth_header((t), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC);

#define http_urlencode_hash_implementation(ht, formstr, argsep) \
	http_urlencode_hash_implementation_ex((ht), (formstr), (argsep), NULL, 0, NULL, 0, NULL, 0, NULL)
#define http_urlencode_hash_implementation_ex(ht, formstr, argsep, np, npl, kp, kpl, ks, ksl, type) \
	_http_urlencode_hash_implementation_ex((ht), (formstr), (argsep), (np), (npl), (kp), (kpl), (ks), (ksl), (type) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_implementation_ex(
	HashTable *ht, phpstr *formstr, char *arg_sep,
	const char *num_prefix, int num_prefix_len,
	const char *key_prefix, int key_prefix_len,
	const char *key_suffix, int key_suffix_len,
	zval *type TSRMLS_DC);

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

