#ifndef PHP_HTTP_CURL_H
#define PHP_HTTP_CURL_H

PHP_HTTP_API php_http_request_ops_t *php_http_curl_get_request_ops(void);
PHP_HTTP_API php_http_request_pool_ops_t *php_http_curl_get_request_pool_ops(void);
PHP_HTTP_API php_http_request_datashare_ops_t *php_http_curl_get_request_datashare_ops(void);

extern PHP_MINIT_FUNCTION(http_curl);
extern PHP_MSHUTDOWN_FUNCTION(http_curl);
extern PHP_RINIT_FUNCTION(http_curl);

#ifdef PHP_HTTP_HAVE_EVENT
struct php_http_request_pool_globals {
	void *event_base;
};
#endif

#endif
