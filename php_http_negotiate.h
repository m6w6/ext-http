/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: php_http_headers_api.h 300300 2010-06-09 07:29:35Z mike $ */

#ifndef PHP_HTTP_NEGOTIATE_H
#define PHP_HTTP_NEGOTIATE_H

typedef char *(*php_http_negotiate_func_t)(const char *test, double *quality, HashTable *supported TSRMLS_DC);

extern char *php_http_negotiate_language_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);
extern char *php_http_negotiate_default_func(const char *test, double *quality, HashTable *supported TSRMLS_DC);

PHP_HTTP_API HashTable *php_http_negotiate(const char *value, HashTable *supported, php_http_negotiate_func_t neg TSRMLS_DC);

static inline HashTable *php_http_negotiate_language(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Language") TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, supported, php_http_negotiate_language_func TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_encoding(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Encoding") TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, supported, NULL TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_charset(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Charset") TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, supported, NULL TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_content_type(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept") TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, supported, NULL TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
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

