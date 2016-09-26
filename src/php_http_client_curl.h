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

#ifndef PHP_HTTP_CLIENT_CURL_H
#define PHP_HTTP_CLIENT_CURL_H

#if PHP_HTTP_HAVE_LIBCURL

struct php_http_client_curl_globals {
	php_http_client_driver_t driver;
};

typedef struct php_http_client_curl_handle {
	CURLM *multi;
	CURLSH *share;
} php_http_client_curl_handle_t;

typedef struct php_http_client_curl_ops {
	void *(*init)();
	void (*dtor)(void **ctx_ptr);
	ZEND_RESULT_CODE (*once)(void *ctx);
	ZEND_RESULT_CODE (*wait)(void *ctx, struct timeval *custom_timeout);
	ZEND_RESULT_CODE (*exec)(void *ctx);
} php_http_client_curl_ops_t;

typedef struct php_http_client_curl {
	php_http_client_curl_handle_t *handle;

	int unfinished;  /* int because of curl_multi_perform() */

	void *ev_ctx;
	php_http_client_curl_ops_t *ev_ops;
} php_http_client_curl_t;

static inline void php_http_client_curl_get_timeout(php_http_client_curl_t *curl, long max_tout, struct timeval *timeout)
{
	if ((CURLM_OK == curl_multi_timeout(curl->handle->multi, &max_tout)) && (max_tout > 0)) {
		timeout->tv_sec = max_tout / 1000;
		timeout->tv_usec = (max_tout % 1000) * 1000;
	} else {
		timeout->tv_sec = 0;
		timeout->tv_usec = 1000;
	}
}

PHP_HTTP_API void php_http_client_curl_responsehandler(php_http_client_t *client);
PHP_HTTP_API void php_http_client_curl_loop(php_http_client_t *client, curl_socket_t s, int curl_action);
PHP_HTTP_API php_http_client_ops_t *php_http_client_curl_get_ops(void);

PHP_MINIT_FUNCTION(http_client_curl);
PHP_MSHUTDOWN_FUNCTION(http_client_curl);

#endif /* PHP_HTTP_HAVE_LIBCURL */

#endif /* PHP_HTTP_CLIENT_CURL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
