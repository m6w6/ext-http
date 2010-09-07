
#ifndef PHP_HTTP_PROPERTY_PROXY_H
#define PHP_HTTP_PROPERTY_PROXY_H

typedef struct php_http_property_proxy {
	zval *myself;
	zval *object;
	zval *member;
} php_http_property_proxy_t;

PHP_HTTP_API php_http_property_proxy_t *php_http_property_proxy_init(php_http_property_proxy_t *proxy, zval *object, zval *member TSRMLS_DC);
PHP_HTTP_API void php_http_property_proxy_dtor(php_http_property_proxy_t *proxy);
PHP_HTTP_API void php_http_property_proxy_free(php_http_property_proxy_t **proxy);

typedef struct php_http_property_proxy_object {
	zend_object zo;
	php_http_property_proxy_t *proxy;
} php_http_property_proxy_object_t;

extern zend_class_entry *php_http_property_proxy_class_entry;
extern zend_function_entry php_http_property_proxy_method_entry[];

extern zend_object_value php_http_property_proxy_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_property_proxy_object_new_ex(zend_class_entry *ce, php_http_property_proxy_t *proxy, php_http_property_proxy_object_t **ptr TSRMLS_DC);
extern void php_http_property_proxy_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpPropertyProxy, __construct);

PHP_MINIT_FUNCTION(http_property_proxy);

#endif /* PHP_HTTP_PROPERTY_PROXY_H_ */
