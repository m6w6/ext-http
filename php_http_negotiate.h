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

#ifndef PHP_HTTP_NEGOTIATE_H
#define PHP_HTTP_NEGOTIATE_H

PHP_HTTP_API HashTable *php_http_negotiate(const char *value_str, size_t value_len, HashTable *supported, const char *primary_sep_str, size_t primary_sep_len TSRMLS_DC);

static inline HashTable *php_http_negotiate_language(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Language"), &length TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, "-", 1 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_encoding(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Encoding"), &length TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_charset(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Charset"), &length TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_content_type(HashTable *supported TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept"), &length TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, "/", 1 TSRMLS_CC);
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

