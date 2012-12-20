/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_HEADERS_H
#define PHP_HTTP_HEADERS_H

#include "php_http_info.h"

PHP_HTTP_API STATUS php_http_headers_parse(const char *header, size_t length, HashTable *headers, php_http_info_callback_t callback_func, void **callback_data TSRMLS_DC);

PHP_HTTP_API void php_http_headers_to_callback(HashTable *headers, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg TSRMLS_DC);
PHP_HTTP_API void php_http_headers_to_string(php_http_buffer_t *str, HashTable *headers TSRMLS_DC);

PHP_HTTP_API zval *php_http_header_value_to_string(zval *header TSRMLS_DC);

zend_class_entry *php_http_header_get_class_entry(void);

PHP_METHOD(HttpHeader, __construct);
PHP_METHOD(HttpHeader, serialize);
PHP_METHOD(HttpHeader, unserialize);
PHP_METHOD(HttpHeader, match);
PHP_METHOD(HttpHeader, negotiate);
PHP_METHOD(HttpHeader, getParams);
PHP_METHOD(HttpHeader, parse);

PHP_MINIT_FUNCTION(http_header);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

