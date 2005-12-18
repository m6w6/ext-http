/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_URL_API_H
#define PHP_HTTP_URL_API_H

#include "ext/standard/url.h"

#define http_absolute_url(u) _http_absolute_url((u) TSRMLS_CC)
PHP_HTTP_API char *_http_absolute_url(const char *url TSRMLS_DC);

#define http_build_url(o, n, p, s, l) _http_build_url((o), (n), (p), (s), (l) TSRMLS_CC)
PHP_HTTP_API void _http_build_url(const php_url *old_url, const php_url *new_url, php_url **url_ptr, char **url_str, size_t *url_len TSRMLS_DC);

#define http_urlencode_hash(h, q) _http_urlencode_hash_ex((h), 1, NULL, 0, (q), NULL TSRMLS_CC)
#define http_urlencode_hash_ex(h, o, p, pl, q, ql) _http_urlencode_hash_ex((h), (o), (p), (pl), (q), (ql) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, zend_bool override_argsep, char *pre_encoded_data, size_t pre_encoded_len, char **encoded_data, size_t *encoded_len TSRMLS_DC);

#define http_urlencode_hash_recursive(ht, s, as, al, pr, pl) _http_urlencode_hash_recursive((ht), (s), (as), (al), (pr), (pl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_urlencode_hash_recursive(HashTable *ht, phpstr *str, const char *arg_sep, size_t arg_sep_len, const char *prefix, size_t prefix_len TSRMLS_DC);

#define array2url(ht) _array2url((ht) TSRMLS_CC)
static inline php_url *_array2url(HashTable *ht TSRMLS_DC)
{
	zval **e;
	php_url *url = ecalloc(1, sizeof(php_url));
	
	if ((SUCCESS == zend_hash_find(ht, "scheme", sizeof("scheme"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->scheme = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "user", sizeof("user"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->user = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "pass", sizeof("pass"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->pass = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "host", sizeof("host"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->host = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "path", sizeof("path"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->path = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "query", sizeof("query"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->query = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if ((SUCCESS == zend_hash_find(ht, "fragment", sizeof("fragment"), (void **) &e))
			&& (Z_TYPE_PP(e) == IS_STRING) && Z_STRLEN_PP(e)) {
		url->fragment = estrndup(Z_STRVAL_PP(e), Z_STRLEN_PP(e));
	}
	if (SUCCESS == zend_hash_find(ht, "port", sizeof("port"), (void **) &e)) {
		if (Z_TYPE_PP(e) == IS_LONG) {
			url->port = (unsigned short) Z_LVAL_PP(e);
		} else {
			zval *o = *e;
			
			convert_to_long_ex(e);
			url->port = (unsigned short) Z_LVAL_PP(e);
			if (o != *e) zval_ptr_dtor(e);
		}
	}
	
	return url;
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

