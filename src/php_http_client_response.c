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

#include "php_http_api.h"

static zend_class_entry *php_http_client_response_class_entry;
zend_class_entry *php_http_get_client_response_class_entry(void)
{
	return php_http_client_response_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientResponse_getCookies, 0, 0, 0)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(0, allowed_extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientResponse, getCookies)
{
	zend_long flags = 0;
	zval *allowed_extras_array = NULL;
	int i = 0;
	char **allowed_extras = NULL;
	zval *header = NULL, *entry = NULL;
	php_http_message_object_t *msg;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|la!/", &flags, &allowed_extras_array)) {
		return;
	}

	msg = PHP_HTTP_OBJ(NULL, getThis());
	array_init(return_value);

	if (allowed_extras_array) {
		/* FIXME: use zend_string** instead of char** */
		allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(allowed_extras_array), entry)
		{
			zend_string *zs = zval_get_string(entry);
			allowed_extras[i++] = estrndup(zs->val, zs->len);
			zend_string_release(zs);
		}
		ZEND_HASH_FOREACH_END();
	}

	if ((header = php_http_message_header(msg->message, ZEND_STRL("Set-Cookie")))) {
		php_http_cookie_list_t *list;

		if (Z_TYPE_P(header) == IS_ARRAY) {
			zval *single_header;

			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(header), single_header)
			{
				zend_string *zs = zval_get_string(single_header);

				if ((list = php_http_cookie_list_parse(NULL, zs->val, zs->len, flags, allowed_extras))) {
					zval cookie;

					ZVAL_OBJ(&cookie, &php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list)->zo);
					add_next_index_zval(return_value, &cookie);
				}
				zend_string_release(zs);
			}
			ZEND_HASH_FOREACH_END();
		} else {
			zend_string *zs = zval_get_string(header);

			if ((list = php_http_cookie_list_parse(NULL, zs->val, zs->len, flags, allowed_extras))) {
				zval cookie;

				ZVAL_OBJ(&cookie, &php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list)->zo);
				add_next_index_zval(return_value, &cookie);
			}
			zend_string_release(zs);
		}
	}

	if (allowed_extras) {
		for (i = 0; allowed_extras[i]; ++i) {
			efree(allowed_extras[i]);
		}
		efree(allowed_extras);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientResponse_getTransferInfo, 0, 0, 0)
	ZEND_ARG_INFO(0, element)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientResponse, getTransferInfo)
{
	char *info_name = NULL;
	size_t info_len = 0;
	zval info_tmp, info_name_tmp, *info;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &info_name, &info_len), invalid_arg, return);

	info = zend_read_property(php_http_client_response_class_entry, getThis(), ZEND_STRL("transferInfo"), 0, &info_tmp);

	/* request completed? */
	if (Z_TYPE_P(info) != IS_OBJECT) {
		php_http_throw(bad_method_call, "Incomplete state", NULL);
		return;
	}

	if (info_len && info_name) {
		info = zend_read_property(NULL, info, php_http_pretty_key(info_name, info_len, 0, 0), info_len, 0, &info_name_tmp);

		if (!info) {
			php_http_throw(unexpected_val, "Could not find transfer info with name '%s'", info_name);
			return;
		}
	}

	RETURN_ZVAL(info, 1, 0);
}

static zend_function_entry php_http_client_response_methods[] = {
	PHP_ME(HttpClientResponse, getCookies,      ai_HttpClientResponse_getCookies,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientResponse, getTransferInfo, ai_HttpClientResponse_getTransferInfo, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client_response)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Client", "Response", php_http_client_response_methods);
	php_http_client_response_class_entry = zend_register_internal_class_ex(&ce, php_http_message_get_class_entry());

	zend_declare_property_null(php_http_client_response_class_entry, ZEND_STRL("transferInfo"), ZEND_ACC_PROTECTED);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

