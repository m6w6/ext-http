/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
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

PHP_HTTP_API zend_class_entry *php_http_querystring_class_entry;

PHP_MINIT_FUNCTION(http_querystring);

#define php_http_querystring_object_new php_http_object_new
#define php_http_querystring_object_new_ex php_http_object_new_ex

#endif /* PHP_HTTP_QUERYSTRING_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
