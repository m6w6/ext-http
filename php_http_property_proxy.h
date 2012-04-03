/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_PROPERTY_PROXY_H
#define PHP_HTTP_PROPERTY_PROXY_H

typedef struct php_http_property_proxy {
	zval *myself;
	zval *object;
	zval *member;
	zval *parent;
} php_http_property_proxy_t;

PHP_HTTP_API php_http_property_proxy_t *php_http_property_proxy_init(php_http_property_proxy_t *proxy, zval *object, zval *member, zval *parent TSRMLS_DC);
PHP_HTTP_API void php_http_property_proxy_dtor(php_http_property_proxy_t *proxy);
PHP_HTTP_API void php_http_property_proxy_free(php_http_property_proxy_t **proxy);

typedef struct php_http_property_proxy_object {
	zend_object zo;
	php_http_property_proxy_t *proxy;
} php_http_property_proxy_object_t;

zend_class_entry *php_http_property_proxy_get_class_entry(void);

zend_object_value php_http_property_proxy_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_property_proxy_object_new_ex(zend_class_entry *ce, php_http_property_proxy_t *proxy, php_http_property_proxy_object_t **ptr TSRMLS_DC);
void php_http_property_proxy_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpPropertyProxy, __construct);

PHP_MINIT_FUNCTION(http_property_proxy);

#endif /* PHP_HTTP_PROPERTY_PROXY_H_ */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

