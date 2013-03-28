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

#include "php_http_api.h"

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientResponse_getCookies, 0, 0, 0)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(0, allowed_extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientResponse, getCookies)
{
	long flags = 0;
	zval *allowed_extras_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|la!", &flags, &allowed_extras_array)) {
		int i = 0;
		char **allowed_extras = NULL;
		zval *header = NULL, **entry = NULL;
		HashPosition pos;
		php_http_message_object_t *msg = zend_object_store_get_object(getThis() TSRMLS_CC);


		array_init(return_value);

		if (allowed_extras_array) {
			allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
			FOREACH_VAL(pos, allowed_extras_array, entry) {
				zval *data = php_http_ztyp(IS_STRING, *entry);
				allowed_extras[i++] = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
				zval_ptr_dtor(&data);
			}
		}

		if ((header = php_http_message_header(msg->message, ZEND_STRL("Set-Cookie"), 0))) {
			php_http_cookie_list_t *list;

			if (Z_TYPE_P(header) == IS_ARRAY) {
				zval **single_header;

				FOREACH_VAL(pos, header, single_header) {
					zval *data = php_http_ztyp(IS_STRING, *single_header);

					if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), Z_STRLEN_P(data), flags, allowed_extras TSRMLS_CC))) {
						zval *cookie;

						MAKE_STD_ZVAL(cookie);
						ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list, NULL TSRMLS_CC), 0);
						add_next_index_zval(return_value, cookie);
					}
					zval_ptr_dtor(&data);
				}
			} else {
				zval *data = php_http_ztyp(IS_STRING, header);
				if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), Z_STRLEN_P(data), flags, allowed_extras TSRMLS_CC))) {
					zval *cookie;

					MAKE_STD_ZVAL(cookie);
					ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_get_class_entry(), list, NULL TSRMLS_CC), 0);
					add_next_index_zval(return_value, cookie);
				}
				zval_ptr_dtor(&data);
			}
			zval_ptr_dtor(&header);
		}

		if (allowed_extras) {
			for (i = 0; allowed_extras[i]; ++i) {
				efree(allowed_extras[i]);
			}
			efree(allowed_extras);
		}

		return;
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpClientResponse_getTransferInfo, 0, 0, 0)
	ZEND_ARG_INFO(0, element)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpClientResponse, getTransferInfo)
{
	char *info_name = NULL;
	int info_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
		zval **infop, *info = zend_read_property(php_http_client_response_get_class_entry(), getThis(), ZEND_STRL("transferInfo"), 0 TSRMLS_CC);

		/* request completed? */
		if (Z_TYPE_P(info) == IS_ARRAY) {
			if (info_len && info_name) {
				if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(info), php_http_pretty_key(info_name, info_len, 0, 0), info_len + 1, (void *) &infop)) {
					RETURN_ZVAL(*infop, 1, 0);
				} else {
					php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Could not find transfer info named %s", info_name);
				}
			} else {
				RETURN_ZVAL(info, 1, 0);
			}
		}
	}
	RETURN_FALSE;
}

static zend_class_entry *php_http_client_response_class_entry;

zend_class_entry *php_http_client_response_get_class_entry(void)
{
	return php_http_client_response_class_entry;
}

static zend_function_entry php_http_client_response_method_entry[] = {
	PHP_ME(HttpClientResponse, getCookies, ai_HttpClientResponse_getCookies, ZEND_ACC_PUBLIC)
	PHP_ME(HttpClientResponse, getTransferInfo, ai_HttpClientResponse_getTransferInfo, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_client_response)
{
	PHP_HTTP_REGISTER_CLASS(http\\Client, Response, http_client_response, php_http_message_get_class_entry(), 0);

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

