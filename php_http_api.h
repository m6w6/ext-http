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

#ifndef ZEND_ENGINE_2
#	include "php_http_build_query.h"
#endif

#define RETURN_SUCCESS(v)	RETURN_BOOL(SUCCESS == (v))
#define HASH_ORNULL(z) 		((z) ? Z_ARRVAL_P(z) : NULL)
#define NO_ARGS 			if (ZEND_NUM_ARGS()) WRONG_PARAM_COUNT

#define array_copy(src, dst)	zend_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *))
#define array_merge(src, dst)	zend_hash_merge(Z_ARRVAL_P(dst), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *), 1)

#ifdef ZEND_ENGINE_2

#	define HTTP_REGISTER_CLASS_EX(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _class_methods); \
		ce.create_object = name## _new_object; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
		memcpy(& name## _object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
		name## _object_handlers.clone_obj = NULL; \
		name## _declare_default_properties(name## _ce); \
	}

#	define HTTP_REGISTER_CLASS(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _class_methods); \
		ce.create_object = NULL; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
	}

#	define getObject(t, o) t * o = ((t *) zend_object_store_get_object(getThis() TSRMLS_CC))
#	define OBJ_PROP(o) o->zo.properties
#	define DCL_PROP(a, t, n, v) zend_declare_property_ ##t(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_Z(a, n, v) zend_declare_property(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_N(a, n) zend_declare_property_null(ce, (#n), sizeof(#n), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define UPD_PROP(o, t, n, v) zend_update_property_ ##t(o->zo.ce, getThis(), (#n), sizeof(#n), (v) TSRMLS_CC)
#	define SET_PROP(o, n, z) zend_update_property(o->zo.ce, getThis(), (#n), sizeof(#n), (z) TSRMLS_CC)
#	define GET_PROP(o, n) zend_read_property(o->zo.ce, getThis(), (#n), sizeof(#n), 0 TSRMLS_CC)

#	define INIT_PARR(o, n) \
	{ \
		zval *__tmp; \
		MAKE_STD_ZVAL(__tmp); \
		array_init(__tmp); \
		SET_PROP(o, n, __tmp); \
	}

#	define FREE_PARR(o, p) \
	{ \
		zval *__tmp = NULL; \
		if (__tmp = GET_PROP(o, p)) { \
			zval_dtor(__tmp); \
			FREE_ZVAL(__tmp); \
			__tmp = NULL; \
		} \
	}

#endif /* ZEND_ENGINE_2 */

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

/* CR LF */
#define HTTP_CRLF "\r\n"

/* default cache control */
#define HTTP_DEFAULT_CACHECONTROL "private, must-revalidate, max-age=0"

/* max URI length */
#define HTTP_URI_MAXLEN 2048

/* buffer size */
#define HTTP_BUF_SIZE 2097152

/* server vars shorthand */
#define HTTP_SERVER_VARS Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER])


/* {{{ HTTP_GSC(var, name, ret) */
#define HTTP_GSC(var, name, ret)  HTTP_GSP(var, name, return ret)
/* }}} */

/* {{{ HTTP_GSP(var, name, ret) */
#define HTTP_GSP(var, name, ret) \
		if (!(var = http_get_server_var(name))) { \
			ret; \
		} \
		if (!Z_STRLEN_P(var)) { \
			ret; \
		}
/* }}} */

char *pretty_key(char *key, int key_len, int uctitle, int xhyphen);

/* {{{ public API */
#define http_date(t) _http_date((t) TSRMLS_CC)
PHP_HTTP_API char *_http_date(time_t t TSRMLS_DC);

#define http_parse_date(d) _http_parse_date((d))
PHP_HTTP_API time_t _http_parse_date(const char *date);

#define http_send_status(s) _http_send_status((s) TSRMLS_CC)
PHP_HTTP_API inline STATUS _http_send_status(const int status TSRMLS_DC);

#define http_send_header(h) _http_send_header((h) TSRMLS_CC)
PHP_HTTP_API inline STATUS _http_send_header(const char *header TSRMLS_DC);

#define http_send_status_header(s, h) _http_send_status_header((s), (h) TSRMLS_CC)
PHP_HTTP_API inline STATUS _http_send_status_header(const int status, const char *header TSRMLS_DC);

#define http_get_server_var(v) _http_get_server_var((v) TSRMLS_CC)
PHP_HTTP_API inline zval *_http_get_server_var(const char *key TSRMLS_DC);

#define http_etag(p, l, m) _http_etag((p), (l), (m) TSRMLS_CC)
PHP_HTTP_API inline char *_http_etag(const void *data_ptr, const size_t data_len, const http_send_mode data_mode TSRMLS_DC);

#define http_lmod(p, m) _http_lmod((p), (m) TSRMLS_CC)
PHP_HTTP_API inline time_t _http_lmod(const void *data_ptr, const http_send_mode data_mode TSRMLS_DC);

#define http_is_range_request() _http_is_range_request(TSRMLS_C)
PHP_HTTP_API inline int _http_is_range_request(TSRMLS_D);

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

#define http_absolute_uri(url, proto) _http_absolute_uri((url), (proto) TSRMLS_CC)
PHP_HTTP_API char *_http_absolute_uri(const char *url, const char *proto TSRMLS_DC);

#define http_negotiate_language(supported, def) _http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), (def) TSRMLS_CC)
#define http_negotiate_charset(supported, def)  _http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), (def) TSRMLS_CC)
#define http_negotiate_q(e, s, d, t) _http_negotiate_q((e), (s), (d), (t) TSRMLS_CC)
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const zval *supported, const char *def TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(zval *zranges, const size_t length TSRMLS_DC);

#define http_send_ranges(r, d, s, m) _http_send_ranges((r), (d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_ranges(zval *zranges, const void *data, const size_t size, const http_send_mode mode TSRMLS_DC);

#define http_send(d, s, m) _http_send((d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send(const void *data, const size_t data_size, const http_send_mode mode TSRMLS_DC);

#define http_send_data(z) _http_send_data((z) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_data(const zval *zdata TSRMLS_DC);

#define http_send_stream(s) _http_send_stream((s) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_stream(const php_stream *s TSRMLS_DC);

#define http_send_file(f) _http_send_file((f) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_file(const zval *zfile TSRMLS_DC);

#define http_chunked_decode(e, el, d, dl) _http_chunked_decode((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded, const size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_split_response(r, h, b) _http_split_response_ex(Z_STRVAL_P(r), Z_STRLEN_P(r), (h), (b) TSRMLS_CC)
#define http_split_response_ex(r, l, h, b) _http_split_response_ex((r), (l), (h), (b) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response_ex(char *response, size_t repsonse_len, zval *zheaders, zval *zbody TSRMLS_DC);

#define http_parse_headers(h, l, a) _http_parse_headers((h), (l), (a) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers(char *header, int header_len, zval *array TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers((h) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers(zval *array TSRMLS_DC);

#define http_auth_credentials(u, p) _http_auth_credentials((u), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC);

#define http_auth_header(t, r) _http_auth_header((t), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC);

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

