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

#ifndef PHP_HTTP_ENV_RESPONSE_H
#define PHP_HTTP_ENV_RESPONSE_H

typedef struct php_http_env_response php_http_env_response_t;

typedef struct php_http_env_response_ops {
	ZEND_RESULT_CODE (*init)(php_http_env_response_t *r, void *arg);
	void (*dtor)(php_http_env_response_t *r);
	long (*get_status)(php_http_env_response_t *r);
	ZEND_RESULT_CODE (*set_status)(php_http_env_response_t *r, long http_code);
	ZEND_RESULT_CODE (*set_protocol_version)(php_http_env_response_t *r, php_http_version_t *v);
	ZEND_RESULT_CODE (*set_header)(php_http_env_response_t *r, const char *fmt, ...);
	ZEND_RESULT_CODE (*add_header)(php_http_env_response_t *r, const char *fmt, ...);
	ZEND_RESULT_CODE (*del_header)(php_http_env_response_t *r, const char *header_str, size_t header_len);
	ZEND_RESULT_CODE (*write)(php_http_env_response_t *r, const char *data_str, size_t data_len);
	ZEND_RESULT_CODE (*flush)(php_http_env_response_t *r);
	ZEND_RESULT_CODE (*finish)(php_http_env_response_t *r);
} php_http_env_response_ops_t;

PHP_HTTP_API php_http_env_response_ops_t *php_http_env_response_get_sapi_ops(void);
PHP_HTTP_API php_http_env_response_ops_t *php_http_env_response_get_stream_ops(void);

struct php_http_env_response {
	void *ctx;
	php_http_env_response_ops_t *ops;

	php_http_cookie_list_t *cookies;
	php_http_buffer_t *buffer;
	zval options;

	struct {
		size_t chunk;
		double delay;
	} throttle;

	struct {
		php_http_range_status_t status;
		HashTable values;
		char boundary[32];
	} range;

	struct {
		size_t length;
		char *type;
		char *encoding;

		php_http_encoding_stream_t *encoder;
	} content;

	zend_bool done;
};

PHP_HTTP_API php_http_env_response_t *php_http_env_response_init(php_http_env_response_t *r, zval *options, php_http_env_response_ops_t *ops, void *ops_ctx);
PHP_HTTP_API ZEND_RESULT_CODE php_http_env_response_send(php_http_env_response_t *r);
PHP_HTTP_API void php_http_env_response_dtor(php_http_env_response_t *r);
PHP_HTTP_API void php_http_env_response_free(php_http_env_response_t **r);

PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_etag(zval *options, const char *header_str, size_t header_len, php_http_message_t *request);
PHP_HTTP_API php_http_cache_status_t php_http_env_is_response_cached_by_last_modified(zval *options, const char *header_str, size_t header_len, php_http_message_t *request);

PHP_HTTP_API zend_class_entry *php_http_get_env_response_class_entry();
PHP_MINIT_FUNCTION(http_env_response);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

