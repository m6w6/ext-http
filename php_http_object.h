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

#ifndef PHP_HTTP_OBJECT_H
#define PHP_HTTP_OBJECT_H

typedef zend_object_value (*php_http_new_t)(zend_class_entry *ce, void *, void ** TSRMLS_DC);

PHP_HTTP_API STATUS php_http_new(zend_object_value *ov, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC);

typedef struct php_http_object {
	zend_object zo;
} php_http_object_t;

zend_class_entry *php_http_object_get_class_entry(void);

PHP_MINIT_FUNCTION(http_object);

zend_object_value php_http_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_object_new_ex(zend_class_entry *ce, void *nothing, php_http_object_t **ptr TSRMLS_DC);

PHP_HTTP_API zend_error_handling_t php_http_object_get_error_handling(zval *object TSRMLS_DC);

PHP_METHOD(HttpObject, setErrorHandling);
PHP_METHOD(HttpObject, getErrorHandling);
PHP_METHOD(HttpObject, setDefaultErrorHandling);
PHP_METHOD(HttpObject, getDefaultErrorHandling);
PHP_METHOD(HttpObject, triggerError);

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

