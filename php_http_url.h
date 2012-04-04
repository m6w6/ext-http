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

#ifndef PHP_HTTP_URL_H
#define PHP_HTTP_URL_H

#include <ext/standard/url.h>

#define PHP_HTTP_URL_REPLACE		0x000
#define PHP_HTTP_URL_JOIN_PATH		0x001
#define PHP_HTTP_URL_JOIN_QUERY		0x002
#define PHP_HTTP_URL_STRIP_USER		0x004
#define PHP_HTTP_URL_STRIP_PASS		0x008
#define PHP_HTTP_URL_STRIP_AUTH		(PHP_HTTP_URL_STRIP_USER|PHP_HTTP_URL_STRIP_PASS)
#define PHP_HTTP_URL_STRIP_PORT		0x020
#define PHP_HTTP_URL_STRIP_PATH		0x040
#define PHP_HTTP_URL_STRIP_QUERY	0x080
#define PHP_HTTP_URL_STRIP_FRAGMENT	0x100
#define PHP_HTTP_URL_STRIP_ALL ( \
	PHP_HTTP_URL_STRIP_AUTH | \
	PHP_HTTP_URL_STRIP_PORT | \
	PHP_HTTP_URL_STRIP_PATH | \
	PHP_HTTP_URL_STRIP_QUERY | \
	PHP_HTTP_URL_STRIP_FRAGMENT \
)
#define PHP_HTTP_URL_FROM_ENV		0x1000

PHP_HTTP_API void php_http_url(int flags, const php_url *old_url, const php_url *new_url, php_url **url_ptr, char **url_str, size_t *url_len TSRMLS_DC);

PHP_HTTP_API STATUS php_http_url_encode_hash(HashTable *hash, const char *pre_encoded_str, size_t pre_encoded_len, char **encoded_str, size_t *encoded_len TSRMLS_DC);
PHP_HTTP_API STATUS php_http_url_encode_hash_ex(HashTable *ht, php_http_buffer_t *str, const char *arg_sep_str, size_t arg_sep_len, const char *val_sep_str, size_t val_sep_len, const char *prefix_str, size_t prefix_len TSRMLS_DC);

static inline void php_http_url_argsep(const char **str, size_t *len TSRMLS_DC)
{
	if (SUCCESS != php_http_ini_entry(ZEND_STRL("arg_separator.output"), str, len, 0 TSRMLS_CC) || !*len) {
		*str = PHP_HTTP_URL_ARGSEP;
		*len = lenof(PHP_HTTP_URL_ARGSEP);
	}
}

static inline php_url *php_http_url_from_struct(php_url *url, HashTable *ht TSRMLS_DC)
{
	zval **e;
	
	if (!url) {
		url = emalloc(sizeof(*url));
	}
	memset(url, 0, sizeof(*url));
	
	if (SUCCESS == zend_hash_find(ht, "scheme", sizeof("scheme"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->scheme = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "user", sizeof("user"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->user = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "pass", sizeof("pass"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->pass = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "host", sizeof("host"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->host = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "path", sizeof("path"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->path = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "query", sizeof("query"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->query = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "fragment", sizeof("fragment"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_STRING, *e);
		url->fragment = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "port", sizeof("port"), (void *) &e)) {
		zval *cpy = php_http_ztyp(IS_LONG, *e);
		url->port = (unsigned short) Z_LVAL_P(cpy);
		zval_ptr_dtor(&cpy);
	}
	
	return url;
}

static inline HashTable *php_http_url_to_struct(php_url *url, zval *strct TSRMLS_DC)
{
	zval arr;
	
	if (strct) {
		switch (Z_TYPE_P(strct)) {
			default:
				zval_dtor(strct);
				array_init(strct);
				/* no break */
			case IS_ARRAY:
			case IS_OBJECT:
				INIT_PZVAL_ARRAY((&arr), HASH_OF(strct));
				break;
		}
	} else {
		INIT_PZVAL(&arr);
		array_init(&arr);
	}
	
	if (url) {
		if (url->scheme) {
			add_assoc_string(&arr, "scheme", url->scheme, 1);
		}
		if (url->user) {
			add_assoc_string(&arr, "user", url->user, 1);
		}
		if (url->pass) {
			add_assoc_string(&arr, "pass", url->pass, 1);
		}
		if (url->host) {
			add_assoc_string(&arr, "host", url->host, 1);
		}
		if (url->port) {
			add_assoc_long(&arr, "port", (long) url->port);
		}
		if (url->path) {
			add_assoc_string(&arr, "path", url->path, 1);
		}
		if (url->query) {
			add_assoc_string(&arr, "query", url->query, 1);
		}
		if (url->fragment) {
			add_assoc_string(&arr, "fragment", url->fragment, 1);
		}
	}
	
	return Z_ARRVAL(arr);
}

zend_class_entry *php_http_url_get_class_entry(void);

#define php_http_url_object_new php_http_object_new
#define php_http_url_object_new_ex php_http_object_new_ex

PHP_METHOD(HttpUrl, __construct);
PHP_METHOD(HttpUrl, mod);
PHP_METHOD(HttpUrl, toString);
PHP_METHOD(HttpUrl, toArray);

PHP_MINIT_FUNCTION(http_url);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

