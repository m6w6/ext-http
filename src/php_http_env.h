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

#ifndef PHP_HTTP_ENV_H
#define PHP_HTTP_ENV_H

#include "php_http_message_body.h"
#include "php_http_version.h"

struct php_http_env_globals {
	zval *server_var;
	char *etag_mode;

	struct {
		HashTable *headers;
		php_http_message_body_t *body;
	} request;
};

typedef enum php_http_content_encoding {
	PHP_HTTP_CONTENT_ENCODING_NONE,
	PHP_HTTP_CONTENT_ENCODING_GZIP
} php_http_content_encoding_t;

typedef enum php_http_range_status {
	PHP_HTTP_RANGE_NO,
	PHP_HTTP_RANGE_OK,
	PHP_HTTP_RANGE_ERR
} php_http_range_status_t;

PHP_HTTP_API php_http_range_status_t php_http_env_get_request_ranges(HashTable *ranges, size_t entity_length, php_http_message_t *request);
PHP_HTTP_API void php_http_env_get_request_headers(HashTable *headers);
PHP_HTTP_API char *php_http_env_get_request_header(const char *name_str, size_t name_len, size_t *len, php_http_message_t *request);
PHP_HTTP_API zend_bool php_http_env_got_request_header(const char *name_str, size_t name_len, php_http_message_t *request);
PHP_HTTP_API php_http_message_body_t *php_http_env_get_request_body(void);
PHP_HTTP_API const char *php_http_env_get_request_method(php_http_message_t *request);

typedef enum php_http_content_disposition {
	PHP_HTTP_CONTENT_DISPOSITION_NONE,
	PHP_HTTP_CONTENT_DISPOSITION_INLINE,
	PHP_HTTP_CONTENT_DISPOSITION_ATTACHMENT
} php_http_content_disposition_t;

typedef enum php_http_cache_status {
	PHP_HTTP_CACHE_NO,
	PHP_HTTP_CACHE_HIT,
	PHP_HTTP_CACHE_MISS
} php_http_cache_status_t;

PHP_HTTP_API long php_http_env_get_response_code(void);
PHP_HTTP_API const char *php_http_env_get_response_status_for_code(unsigned code);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_get_response_headers(HashTable *headers_ht);
PHP_HTTP_API char *php_http_env_get_response_header(const char *name_str, size_t name_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_code(long http_code);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_protocol_version(php_http_version_t *v);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_header(long http_code, const char *header_str, size_t header_len, zend_bool replace);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_header_value(long http_code, const char *name_str, size_t name_len, zval *value, zend_bool replace);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_header_format(long http_code, zend_bool replace, const char *fmt, ...);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_set_response_header_va(long http_code, zend_bool replace, const char *fmt, va_list argv);

PHP_HTTP_API zval *php_http_env_get_server_var(const char *key_str, size_t key_len, zend_bool check);
PHP_HTTP_API zval *php_http_env_get_superglobal(const char *key, size_t key_len);

static inline zend_bool php_http_env_got_server_var(const char *v)
{
	return NULL != php_http_env_get_server_var(v, strlen(v), 1);
}


PHP_HTTP_API zend_class_entry *php_http_env_get_class_entry(void);
PHP_MINIT_FUNCTION(http_env);
PHP_RSHUTDOWN_FUNCTION(http_env);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

