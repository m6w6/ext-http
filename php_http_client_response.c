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

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpClientResponse, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpClientResponse, method, 0)
#define PHP_HTTP_CLIENT_RESPONSE_ME(method, visibility)	PHP_ME(HttpClientResponse, method, PHP_HTTP_ARGS(HttpClientResponse, method), visibility)
#define PHP_HTTP_CLIENT_RESPONSE_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpClientResponse, method))
#define PHP_HTTP_CLIENT_RESPONSE_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpClientResponse_##al), PHP_HTTP_ARGS(HttpClientResponse, al), vis)

PHP_HTTP_BEGIN_ARGS(getCookies, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
	PHP_HTTP_ARG_VAL(allowed_extras, 0)
PHP_HTTP_END_ARGS;

PHP_METHOD(HttpClientResponse, getCookies)
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


static zend_class_entry *php_http_client_response_class_entry;

zend_class_entry *php_http_client_response_get_class_entry(void)
{
	return php_http_client_response_class_entry;
}

static zend_function_entry php_http_client_response_method_entry[] = {
	PHP_HTTP_CLIENT_RESPONSE_ME(getCookies, ZEND_ACC_PUBLIC)
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

