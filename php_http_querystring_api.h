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

#ifndef PHP_HTTP_QUERYSTRING_API_H
#define PHP_HTTP_QUERYSTRING_API_H

#ifdef HTTP_HAVE_ICONV
#define http_querystring_xlate(a, p, ie, oe) _http_querystring_xlate((a), (p), (ie), (oe) TSRMLS_CC)
PHP_HTTP_API int _http_querystring_xlate(zval *array, zval *param, const char *ie, const char *oe TSRMLS_DC);
#endif

#define http_querystring_update(qa, qs) _http_querystring_update((qa), (qs) TSRMLS_CC)
PHP_HTTP_API void _http_querystring_update(zval *qarray, zval *qstring TSRMLS_DC);

#define http_querystring_modify(q, p) _http_querystring_modify((q), (p) TSRMLS_CC)
PHP_HTTP_API int _http_querystring_modify(zval *qarray, zval *params TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
