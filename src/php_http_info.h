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

#ifndef PHP_HTTP_INFO_H
#define PHP_HTTP_INFO_H

#include "php_http_version.h"
#include "php_http_url.h"

typedef struct php_http_info_data {
	union {
		/* GET /foo/bar */
		struct { char *method; php_http_url_t *url; } request;
		/* 200 Ok */
		struct { unsigned code; char *status; } response;
	} info;
	php_http_version_t version;
} php_http_info_data_t;

#undef PHP_HTTP_REQUEST
#undef PHP_HTTP_RESPONSE
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

typedef zend_bool (*php_http_info_callback_t)(void **callback_data, HashTable **headers, php_http_info_t *info);

PHP_HTTP_API php_http_info_t *php_http_info_init(php_http_info_t *info);
PHP_HTTP_API php_http_info_t *php_http_info_parse(php_http_info_t *info, const char *pre_header);
PHP_HTTP_API void php_http_info_to_string(php_http_info_t *info, char **str, size_t *len, const char *eol);
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

