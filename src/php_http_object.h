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

#ifndef PHP_HTTP_OBJECT_H
#define PHP_HTTP_OBJECT_H

typedef struct php_http_object {
	void *intern;
	zend_object zo;
} php_http_object_t;

zend_object *php_http_object_new(zend_class_entry *ce);
php_http_object_t *php_http_object_new_ex(zend_class_entry *ce, void *nothing);

typedef void *(*php_http_new_t)(zend_class_entry *ce, void *);

ZEND_RESULT_CODE php_http_new(void **obj_ptr, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr);

PHP_MINIT_FUNCTION(http_object);

typedef struct php_http_method {
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
} php_http_object_method_t;

php_http_object_method_t *php_http_object_method_init(php_http_object_method_t *cb, zval *zobject, const char *method_str, size_t method_len);
ZEND_RESULT_CODE php_http_object_method_call(php_http_object_method_t *cb, zval *zobject, zval *retval, int argc, zval *args);
void php_http_object_method_dtor(php_http_object_method_t *cb);
void php_http_object_method_free(php_http_object_method_t **cb);

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

