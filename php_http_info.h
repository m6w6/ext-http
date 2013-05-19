/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_INFO_H
#define PHP_HTTP_INFO_H

#include "php_http_version.h"

#define PHP_HTTP_INFO_REQUEST_FMT_ARGS(_http_ptr, eol) "%s %s HTTP/%u.%u" eol, \
				(_http_ptr)->info.request.method?(_http_ptr)->info.request.method:"UNKNOWN", \
				(_http_ptr)->info.request.url?(_http_ptr)->info.request.url:"/", \
				(_http_ptr)->version.major||(_http_ptr)->version.major?(_http_ptr)->version.major:1, \
				(_http_ptr)->version.major||(_http_ptr)->version.minor?(_http_ptr)->version.minor:1

#define PHP_HTTP_INFO_RESPONSE_FMT_ARGS(_http_ptr, eol) "HTTP/%u.%u %d%s%s" eol, \
				(_http_ptr)->version.major||(_http_ptr)->version.major?(_http_ptr)->version.major:1, \
				(_http_ptr)->version.major||(_http_ptr)->version.minor?(_http_ptr)->version.minor:1, \
				(_http_ptr)->info.response.code?(_http_ptr)->info.response.code:200, \
				(_http_ptr)->info.response.status&&*(_http_ptr)->info.response.status ? " ":"", \
				STR_PTR((_http_ptr)->info.response.status)

typedef struct php_http_info_data {
	union {
		/* GET /foo/bar */
		struct { char *method; char *url; } request;
		/* 200 Ok */
		struct { unsigned code; char *status; } response;
	} info;
	php_http_version_t version;
} php_http_info_data_t;

typedef enum php_http_info_type {
	PHP_HTTP_NONE = 0,
	PHP_HTTP_REQUEST,
	PHP_HTTP_RESPONSE
} php_http_info_type_t;

#define PHP_HTTP_INFO(ptr) (ptr)->http.info
#define PHP_HTTP_INFO_IMPL(_http, _type) \
	php_http_info_data_t _http; \
	php_http_info_type_t _type;

typedef struct php_http_info {
	PHP_HTTP_INFO_IMPL(http, type)
} php_http_info_t;

typedef zend_bool (*php_http_info_callback_t)(void **callback_data, HashTable **headers, php_http_info_t *info TSRMLS_DC);

PHP_HTTP_API php_http_info_t *php_http_info_init(php_http_info_t *info TSRMLS_DC);
PHP_HTTP_API php_http_info_t *php_http_info_parse(php_http_info_t *info, const char *pre_header TSRMLS_DC);
PHP_HTTP_API void php_http_info_dtor(php_http_info_t *info);
PHP_HTTP_API void php_http_info_free(php_http_info_t **info);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

