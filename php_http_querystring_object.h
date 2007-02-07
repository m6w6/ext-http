/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_QUERYSTRING_OBJECT_H
#define PHP_HTTP_QUERYSTRING_OBJECT_H
#ifdef ZEND_ENGINE_2

typedef struct _http_querystring_object_t {
	zend_object zo;
} http_querystring_object;

#define HTTP_QUERYSTRING_TYPE_BOOL		IS_BOOL
#define HTTP_QUERYSTRING_TYPE_INT		IS_LONG
#define HTTP_QUERYSTRING_TYPE_FLOAT		IS_DOUBLE
#define HTTP_QUERYSTRING_TYPE_STRING	IS_STRING
#define HTTP_QUERYSTRING_TYPE_ARRAY		IS_ARRAY
#define HTTP_QUERYSTRING_TYPE_OBJECT	IS_OBJECT

extern zend_class_entry *http_querystring_object_ce;
extern zend_function_entry http_querystring_object_fe[];

extern PHP_MINIT_FUNCTION(http_querystring_object);

#define http_querystring_object_new(ce) _http_querystring_object_new((ce) TSRMLS_CC)
extern zend_object_value _http_querystring_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_querystring_object_new_ex(ce, n, ptr) _http_querystring_object_new_ex((ce), (n), (ptr) TSRMLS_CC)
extern zend_object_value _http_querystring_object_new_ex(zend_class_entry *ce, void *nothing, http_querystring_object **ptr TSRMLS_DC);
#define http_querystring_object_free(o) _http_querystring_object_free((o) TSRMLS_CC)
extern void _http_querystring_object_free(zend_object *object TSRMLS_DC);

PHP_METHOD(HttpQueryString, __construct);
PHP_METHOD(HttpQueryString, toString);
PHP_METHOD(HttpQueryString, toArray);
PHP_METHOD(HttpQueryString, get);
PHP_METHOD(HttpQueryString, set);
PHP_METHOD(HttpQueryString, mod);
PHP_METHOD(HttpQueryString, getBool);
PHP_METHOD(HttpQueryString, getInt);
PHP_METHOD(HttpQueryString, getFloat);
PHP_METHOD(HttpQueryString, getString);
PHP_METHOD(HttpQueryString, getArray);
PHP_METHOD(HttpQueryString, getObject);
#ifdef HTTP_HAVE_ICONV
PHP_METHOD(HttpQueryString, xlate);
#endif
PHP_METHOD(HttpQueryString, factory);
#ifndef WONKY
PHP_METHOD(HttpQueryString, singleton);
#endif
PHP_METHOD(HttpQueryString, serialize);
PHP_METHOD(HttpQueryString, unserialize);
PHP_METHOD(HttpQueryString, offsetGet);
PHP_METHOD(HttpQueryString, offsetSet);
PHP_METHOD(HttpQueryString, offsetExists);
PHP_METHOD(HttpQueryString, offsetUnset);
#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

