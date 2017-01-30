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

#ifndef PHP_HTTP_NEGOTIATE_H
#define PHP_HTTP_NEGOTIATE_H

PHP_HTTP_API HashTable *php_http_negotiate(const char *value_str, size_t value_len, HashTable *supported, const char *primary_sep_str, size_t primary_sep_len);

static inline HashTable *php_http_negotiate_language(HashTable *supported, php_http_message_t *request)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Language"), &length, request);

	if (value) {
		result = php_http_negotiate(value, length, supported, "-", 1);
	}
	PTR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_encoding(HashTable *supported, php_http_message_t *request)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Encoding"), &length, request);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0);
	}
	PTR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_charset(HashTable *supported, php_http_message_t *request)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept-Charset"), &length, request);

	if (value) {
		result = php_http_negotiate(value, length, supported, NULL, 0);
	}
	PTR_FREE(value);

	return result;
}

static inline HashTable *php_http_negotiate_content_type(HashTable *supported, php_http_message_t *request)
{
	HashTable *result = NULL;
	size_t length;
	char *value = php_http_env_get_request_header(ZEND_STRL("Accept"), &length, request);

	if (value) {
		result = php_http_negotiate(value, length, supported, "/", 1);
	}
	PTR_FREE(value);

	return result;
}

#define PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported) \
	{ \
		zval *value; \
		HashPosition pos; \
		 \
		zend_hash_internal_pointer_reset_ex((supported), &pos); \
		if ((value = zend_hash_get_current_data_ex((supported), &pos))) { \
			RETVAL_ZVAL(value, 1, 0); \
		} else { \
			RETVAL_NULL(); \
		} \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array) \
	PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
	if (rs_array) { \
		zval *value; \
		 \
		ZEND_HASH_FOREACH_VAL(supported, value) \
		{ \
			zend_string *zs = zval_get_string(value); \
			add_assoc_double_ex(rs_array, zs->val, zs->len, 1.0); \
			zend_string_release(zs); \
		} \
		ZEND_HASH_FOREACH_END(); \
	}

#define PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(result, supported, rs_array) \
	{ \
		zend_string *key; \
		zend_ulong idx; \
		 \
		if (zend_hash_num_elements(result) && HASH_KEY_IS_STRING == zend_hash_get_current_key(result, &key, &idx)) { \
			RETVAL_STR_COPY(key); \
		} else { \
			PHP_HTTP_DO_NEGOTIATE_DEFAULT(supported); \
		} \
		\
		if (rs_array) { \
			zend_hash_copy(Z_ARRVAL_P(rs_array), result, (copy_ctor_func_t) zval_add_ref); \
		} \
		\
		zend_hash_destroy(result); \
		FREE_HASHTABLE(result); \
	}

#define PHP_HTTP_DO_NEGOTIATE(type, supported, rs_array) \
	{ \
		HashTable *result; \
		if ((result = php_http_negotiate_ ##type(supported, NULL))) { \
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

