#ifndef PHP_HTTP_REQUEST_DATASHARE_H
#define PHP_HTTP_REQUEST_DATASHARE_H

#ifdef ZTS
typedef struct php_http_request_datashare_lock {
	CURL *ch;
	MUTEX_T mx;
} php_http_request_datashare_lock_t;
#endif

typedef union php_http_request_datashare_handle {
	zend_llist *list;
#ifdef ZTS
	php_http_request_datashare_lock_t *locks;
#endif
} php_http_request_datashare_handle_t;

typedef struct php_http_request_datashare_t {
	CURLSH *ch;
	php_http_request_datashare_handle_t handle;
	unsigned persistent:1;
#ifdef ZTS
	void ***ts;
#endif
} php_http_request_datashare_t;

struct php_http_request_datashare_globals {
	zend_llist handles;
	zend_bool cookie;
	zend_bool dns;
	zend_bool ssl;
	zend_bool connect;
};

#define PHP_HTTP_RSHARE_HANDLES(s) ((s)->persistent ? &PHP_HTTP_G->request_datashare.handles : (s)->handle.list)

extern php_http_request_datashare_t *php_http_request_datashare_global_get(void);

extern PHP_MINIT_FUNCTION(http_request_datashare);
extern PHP_MSHUTDOWN_FUNCTION(http_request_datashare);
extern PHP_RINIT_FUNCTION(http_request_datashare);
extern PHP_RSHUTDOWN_FUNCTION(http_request_datashare);

PHP_HTTP_API php_http_request_datashare_t *php_http_request_datashare_init(php_http_request_datashare_t *share, zend_bool persistent TSRMLS_DC);
PHP_HTTP_API STATUS php_http_request_datashare_attach(php_http_request_datashare_t *share, zval *request);
PHP_HTTP_API STATUS php_http_request_datashare_detach(php_http_request_datashare_t *share, zval *request);
PHP_HTTP_API void php_http_request_datashare_detach_all(php_http_request_datashare_t *share);
PHP_HTTP_API void php_http_request_datashare_dtor(php_http_request_datashare_t *share);
PHP_HTTP_API void php_http_request_datashare_free(php_http_request_datashare_t **share);
PHP_HTTP_API STATUS php_http_request_datashare_set(php_http_request_datashare_t *share, const char *option, size_t option_len, zend_bool enable);

typedef struct php_http_request_datashare_object {
	zend_object zo;
	php_http_request_datashare_t *share;
} php_http_request_datashare_object_t;

extern zend_class_entry *php_http_request_datashare_class_entry;
extern zend_function_entry php_http_request_datashare_method_entry[];

extern zend_object_value php_http_request_datashare_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_request_datashare_object_new_ex(zend_class_entry *ce, php_http_request_datashare_t *share, php_http_request_datashare_object_t **ptr TSRMLS_DC);
extern void php_http_request_datashare_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpRequestDataShare, __destruct);
PHP_METHOD(HttpRequestDataShare, count);
PHP_METHOD(HttpRequestDataShare, attach);
PHP_METHOD(HttpRequestDataShare, detach);
PHP_METHOD(HttpRequestDataShare, reset);
PHP_METHOD(HttpRequestDataShare, getGlobalInstance);

#endif /* PHP_HTTP_REQUEST_DATASHARE_H */
