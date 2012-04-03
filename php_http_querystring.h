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

#ifndef PHP_HTTP_QUERYSTRING_H
#define PHP_HTTP_QUERYSTRING_H

#ifdef PHP_HTTP_HAVE_ICONV
PHP_HTTP_API STATUS php_http_querystring_xlate(zval *dst, zval *src, const char *ie, const char *oe TSRMLS_DC);
#endif /* PHP_HTTP_HAVE_ICONV */
PHP_HTTP_API STATUS php_http_querystring_update(zval *qarray, zval *params, zval *qstring TSRMLS_DC);
PHP_HTTP_API STATUS php_http_querystring_ctor(zval *instance, zval *params TSRMLS_DC);

typedef php_http_object_t php_http_querystring_object_t;

#define PHP_HTTP_QUERYSTRING_TYPE_BOOL		IS_BOOL
#define PHP_HTTP_QUERYSTRING_TYPE_INT		IS_LONG
#define PHP_HTTP_QUERYSTRING_TYPE_FLOAT		IS_DOUBLE
#define PHP_HTTP_QUERYSTRING_TYPE_STRING	IS_STRING
#define PHP_HTTP_QUERYSTRING_TYPE_ARRAY		IS_ARRAY
#define PHP_HTTP_QUERYSTRING_TYPE_OBJECT	IS_OBJECT

zend_class_entry *php_http_querystring_get_class_entry(void);

PHP_MINIT_FUNCTION(http_querystring);

#define php_http_querystring_object_new php_http_object_new
#define php_http_querystring_object_new_ex php_http_object_new_ex

PHP_METHOD(HttpQueryString, getGlobalInstance);
PHP_METHOD(HttpQueryString, __construct);
PHP_METHOD(HttpQueryString, getIterator);
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
#ifdef PHP_HTTP_HAVE_ICONV
PHP_METHOD(HttpQueryString, xlate);
#endif /* PHP_HTTP_HAVE_ICONV */
PHP_METHOD(HttpQueryString, factory);
PHP_METHOD(HttpQueryString, singleton);
PHP_METHOD(HttpQueryString, serialize);
PHP_METHOD(HttpQueryString, unserialize);
PHP_METHOD(HttpQueryString, offsetGet);
PHP_METHOD(HttpQueryString, offsetSet);
PHP_METHOD(HttpQueryString, offsetExists);
PHP_METHOD(HttpQueryString, offsetUnset);

#endif /* PHP_HTTP_QUERYSTRING_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
