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

static inline HashTable *php_http_negotiate_language(HashTable *supported, php_http_message_t *request TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Language"), &length, request TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, "-", 1 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_encoding(HashTable *supported, php_http_message_t *request TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Encoding"), &length, request TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_charset(HashTable *supported, php_http_message_t *request TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Charset"), &length, request TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_content_type(HashTable *supported, php_http_message_t *request TSRMLS_DC)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept"), &length, request TSRMLS_CC);

	if (value) {
		result = php_http_negotiate(value, length, supported, "/", 1 TSRMLS_CC);
	}
	STR_FREE(value);

	return result;
}

#define PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported) \
	{ \
		zval **value; \
		 \
		zend_hash_internal_pointer_reset((supported)); \
		if (SUCCESS == zend_hash_get_current_data((supported), (void *) &value)) { \
			RETVAL_ZVAL(*value, 1, 0); \
		} else { \
			RETVAL_NULL(); \
		} \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array) \
	PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
	if (rs_array) { \
		HashPosition pos; \
		zval **value_ptr; \
		 \
		FOREACH_HASH_VAL(pos, supported, value_ptr) { \
			zval *value = php_http_ztyp(IS_STRING, *value_ptr); \
			add_assoc_double(rs_array, Z_STRVAL_P(value), 1.0); \
			zval_ptr_dtor(&value); \
		} \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(result, supported, rs_array) \
	{ \
		char *key; \
		uint key_len; \
		ulong idx; \
		 \
		if (zend_hash_num_elements(result) && HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(result, &key, &key_len, &idx, 1, NULL)) { \
			RETVAL_STRINGL(key, key_len-1, 0); \
		} else { \
			PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
		} \
		\
		if (rs_array) { \
			zend_hash_copy(Z_ARRVAL_P(rs_array), result, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *)); \
		} \
		\
		zend_hash_destroy(result); \
		FREE_HASHTABLE(result); \
	}

#define PHP_HTTP_DO_NEGOTIATE(type, supported, rs_array) \
	{ \
		HashTable *result; \
		if ((result = php_http_negotiate_ ##type(supported, NULL TSRMLS_CC))) { \
			PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(result, supported, rs_array); \
		} else { \
			PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array); \
		} \
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

