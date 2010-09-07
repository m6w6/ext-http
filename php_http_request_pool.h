
#ifndef PHP_HTTP_REQUESTPOOL_H
#define PHP_HTTP_REQUESTPOOL_H

struct php_http_request_pool_globals {
	void *event_base;
};

typedef struct php_http_request_pool {
	CURLM *ch;
	zend_llist finished;
	zend_llist handles;
	int unfinished; /* int because of curl_multi_perform() */
#ifdef ZTS
	void ***ts;
#endif
#ifdef PHP_HTTP_HAVE_EVENT
	struct event *timeout;
	unsigned useevents:1;
	unsigned runsocket:1;
#endif
} php_http_request_pool_t;

typedef int (*php_http_request_pool_apply_func_t)(php_http_request_pool_t *pool, zval *request);
typedef int (*php_http_request_pool_apply_with_arg_func_t)(php_http_request_pool_t *pool, zval *request, void *arg);

#ifdef PHP_HTTP_HAVE_EVENT
PHP_RINIT_FUNCTION(php_http_request_pool);
#endif

extern struct timeval *php_http_request_pool_timeout(php_http_request_pool_t *pool, struct timeval *timeout);
extern void php_http_request_pool_responsehandler(php_http_request_pool_t *pool);
extern int php_http_request_pool_apply_responsehandler(php_http_request_pool_t *pool, zval *req, void *ch);

PHP_HTTP_API php_http_request_pool_t *php_http_request_pool_init(php_http_request_pool_t *pool TSRMLS_DC);
PHP_HTTP_API STATUS php_http_request_pool_attach(php_http_request_pool_t *pool, zval *request);
PHP_HTTP_API STATUS php_http_request_pool_detach(php_http_request_pool_t *pool, zval *request);
PHP_HTTP_API void php_http_request_pool_apply(php_http_request_pool_t *pool, php_http_request_pool_apply_func_t cb);
PHP_HTTP_API void php_http_request_pool_apply_with_arg(php_http_request_pool_t *pool, php_http_request_pool_apply_with_arg_func_t cb, void *arg);
PHP_HTTP_API void php_http_request_pool_detach_all(php_http_request_pool_t *pool);
PHP_HTTP_API STATUS php_http_request_pool_send(php_http_request_pool_t *pool);
PHP_HTTP_API STATUS php_http_request_pool_select(php_http_request_pool_t *pool, struct timeval *custom_timeout);
PHP_HTTP_API int php_http_request_pool_perform(php_http_request_pool_t *pool);
PHP_HTTP_API void php_http_request_pool_dtor(php_http_request_pool_t *pool);
PHP_HTTP_API void php_http_request_pool_free(php_http_request_pool_t **pool);

typedef struct php_http_request_pool_object {
	zend_object zo;
	php_http_request_pool_t pool;
	struct {
		long pos;
	} iterator;
} php_http_request_pool_object_t;

extern zend_class_entry *php_http_request_pool_class_entry;
extern zend_function_entry php_http_request_pool_method_entry[];

extern zend_object_value php_http_request_pool_object_new(zend_class_entry *ce TSRMLS_DC);
extern void php_http_request_pool_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpRequestPool, __construct);
PHP_METHOD(HttpRequestPool, __destruct);
PHP_METHOD(HttpRequestPool, attach);
PHP_METHOD(HttpRequestPool, detach);
PHP_METHOD(HttpRequestPool, send);
PHP_METHOD(HttpRequestPool, reset);
PHP_METHOD(HttpRequestPool, socketPerform);
PHP_METHOD(HttpRequestPool, socketSelect);
PHP_METHOD(HttpRequestPool, valid);
PHP_METHOD(HttpRequestPool, current);
PHP_METHOD(HttpRequestPool, key);
PHP_METHOD(HttpRequestPool, next);
PHP_METHOD(HttpRequestPool, rewind);
PHP_METHOD(HttpRequestPool, count);
PHP_METHOD(HttpRequestPool, getAttachedRequests);
PHP_METHOD(HttpRequestPool, getFinishedRequests);
PHP_METHOD(HttpRequestPool, enablePipelining);
PHP_METHOD(HttpRequestPool, enableEvents);

PHP_MINIT_FUNCTION(http_request_pool);
PHP_RINIT_FUNCTION(http_request_pool);

#endif /* PHP_HTTP_REQUESTPOOL_H */
