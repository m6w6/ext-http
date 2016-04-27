/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_HEADERS_H
#define PHP_HTTP_HEADERS_H

#include "php_http_info.h"

PHP_HTTP_API ZEND_RESULT_CODE php_http_header_parse(const char *header, size_t length, HashTable *headers, php_http_info_callback_t callback_func, void **callback_data);

PHP_HTTP_API void php_http_header_to_callback(HashTable *headers, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg);
PHP_HTTP_API void php_http_header_to_callback_ex(const char *key, zval *val, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg);
PHP_HTTP_API void php_http_header_to_string(php_http_buffer_t *str, HashTable *headers);
PHP_HTTP_API void php_http_header_to_string_ex(php_http_buffer_t *str, const char *key, zval *val);

PHP_HTTP_API zend_string *php_http_header_value_to_string(zval *header);
PHP_HTTP_API zend_string *php_http_header_value_array_to_string(zval *header);

PHP_HTTP_API zend_class_entry *php_http_header_get_class_entry(void);
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

