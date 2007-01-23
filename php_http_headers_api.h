/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_HEADERS_API_H
#define PHP_HTTP_HEADERS_API_H

#include "php_http_info_api.h"

typedef enum http_range_status_t {
	RANGE_OK,
	RANGE_NO,
	RANGE_ERR
} http_range_status;

#define http_parse_headers(h, a) _http_parse_headers_ex((h), Z_ARRVAL_P(a), 1, http_info_default_callback, NULL TSRMLS_CC)
#define http_parse_headers_ex(h, ht, p) _http_parse_headers_ex((h), (ht), (p), http_info_default_callback, NULL TSRMLS_CC)
#define http_parse_headers_cb(h, ht, p, f, d) _http_parse_headers_ex((h), (ht), (p), (f), (d) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers_ex(const char *header, HashTable *headers, zend_bool prettify, http_info_callback callback_func, void **callback_data TSRMLS_DC);

typedef char *(*negotiate_func_t)(const char *test, double *quality, HashTable *supported TSRMLS_DC);

#define http_negotiate_language_func _http_negotiate_language_func
extern char *_http_negotiate_language_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);
#define http_negotiate_content_type_func _http_negotiate_default_func
#define http_negotiate_encoding_func _http_negotiate_default_func
#define http_negotiate_charset_func _http_negotiate_default_func
#define http_negotiate_default_func _http_negotiate_default_func
extern char *_http_negotiate_default_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);

#define http_negotiate_language(zsupported) http_negotiate_language_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_language_ex(supported) http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), http_negotiate_language_func)
#define http_negotiate_charset(zsupported) http_negotiate_charset_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_charset_ex(supported)  http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), http_negotiate_charset_func)
#define http_negotiate_encoding(zsupported) http_negotiate_encoding_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_encoding_ex(supported)  http_negotiate_q("HTTP_ACCEPT_ENCODING", (supported), http_negotiate_encoding_func)
#define http_negotiate_content_type(zsupported) http_negotiate_content_type_ex(Z_ARRVAL_P(zsupported))
#define http_negotiate_content_type_ex(supported) http_negotiate_q("HTTP_ACCEPT", (supported), http_negotiate_content_type_func)
#define http_negotiate_q(e, s, n) _http_negotiate_q((e), (s), (n) TSRMLS_CC)
PHP_HTTP_API HashTable *_http_negotiate_q(const char *header, HashTable *supported, negotiate_func_t neg TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers((h) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers(HashTable *headers TSRMLS_DC);

#define http_get_response_headers(h) _http_get_response_headers((h) TSRMLS_CC)
PHP_HTTP_API STATUS _http_get_response_headers(HashTable *headers_ht TSRMLS_DC);

#define http_match_request_header(h, v) http_match_request_header_ex((h), (v), 0)
#define http_match_request_header_ex(h, v, c) _http_match_request_header_ex((h), (v), (c) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_match_request_header_ex(const char *header, const char *value, zend_bool match_case TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

