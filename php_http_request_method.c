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

/* $Id: http_request_method_api.c 292841 2009-12-31 08:48:57Z mike $ */

#include "php_http.h"

#include <Zend/zend_interfaces.h>

static PHP_HTTP_STRLIST(php_http_request_methods) =
	PHP_HTTP_STRLIST_ITEM("UNKNOWN")
	/* HTTP/1.1 */
	PHP_HTTP_STRLIST_ITEM("GET")
	PHP_HTTP_STRLIST_ITEM("HEAD")
	PHP_HTTP_STRLIST_ITEM("POST")
	PHP_HTTP_STRLIST_ITEM("PUT")
	PHP_HTTP_STRLIST_ITEM("DELETE")
	PHP_HTTP_STRLIST_ITEM("OPTIONS")
	PHP_HTTP_STRLIST_ITEM("TRACE")
	PHP_HTTP_STRLIST_ITEM("CONNECT")
	/* WebDAV - RFC 2518 */
	PHP_HTTP_STRLIST_ITEM("PROPFIND")
	PHP_HTTP_STRLIST_ITEM("PROPPATCH")
	PHP_HTTP_STRLIST_ITEM("MKCOL")
	PHP_HTTP_STRLIST_ITEM("COPY")
	PHP_HTTP_STRLIST_ITEM("MOVE")
	PHP_HTTP_STRLIST_ITEM("LOCK")
	PHP_HTTP_STRLIST_ITEM("UNLOCK")
	/* WebDAV Versioning - RFC 3253 */
	PHP_HTTP_STRLIST_ITEM("VERSION-CONTROL")
	PHP_HTTP_STRLIST_ITEM("REPORT")
	PHP_HTTP_STRLIST_ITEM("CHECKOUT")
	PHP_HTTP_STRLIST_ITEM("CHECKIN")
	PHP_HTTP_STRLIST_ITEM("UNCHECKOUT")
	PHP_HTTP_STRLIST_ITEM("MKWORKSPACE")
	PHP_HTTP_STRLIST_ITEM("UPDATE")
	PHP_HTTP_STRLIST_ITEM("LABEL")
	PHP_HTTP_STRLIST_ITEM("MERGE")
	PHP_HTTP_STRLIST_ITEM("BASELINE-CONTROL")
	PHP_HTTP_STRLIST_ITEM("MKACTIVITY")
	/* WebDAV Access Control - RFC 3744 */
	PHP_HTTP_STRLIST_ITEM("ACL")
	PHP_HTTP_STRLIST_STOP
;

PHP_HTTP_API const char *php_http_request_method_name(php_http_request_method_t meth)
{
	if (meth > PHP_HTTP_NO_REQUEST_METHOD && meth < PHP_HTTP_MAX_REQUEST_METHOD) {
		return php_http_strlist_find(php_http_request_methods, 1, meth);
	} else {
		zval **val, *cmp, res;
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

		INIT_PZVAL(&res);
		FOREACH_HASH_KEYVAL(pos, &php_http_request_class_entry->constants_table, key, val) {
			MAKE_STD_ZVAL(cmp);
			ZVAL_LONG(cmp, meth);
			is_equal_function(&res, *val, cmp TSRMLS_CC);
			zval_ptr_dtor(&cmp);

			if (Z_LVAL(res)) {
				return key.str;
			}
		}
	}
	return NULL;
}

PHP_HTTP_API STATUS php_http_request_method_register(const char *meth_str, size_t meth_len, long *id TSRMLS_DC)
{
	long num = zend_hash_num_elements(&php_http_request_class_entry->constants_table);

	if (SUCCESS == zend_declare_class_constant_long(php_http_request_method_class_entry, meth_str, meth_len, num TSRMLS_CC)) {
		if (id) {
			*id = num;
		}
		return SUCCESS;
	}
	return FAILURE;
}

zend_class_entry *php_http_request_method_class_entry;

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequestMethod, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequestMethod, method, 0)
#define PHP_HTTP_REQMETH_ME(method, visibility)	PHP_ME(HttpRequestMethod, method, PHP_HTTP_ARGS(HttpRequestMethod, method), visibility)

#ifdef register
#	undef register
#endif

PHP_HTTP_BEGIN_ARGS(__construct, 1)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(__toString);

PHP_HTTP_EMPTY_ARGS(getId);

PHP_HTTP_BEGIN_ARGS(exists, 1)
	PHP_HTTP_ARG_VAL(method, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(register, 1)
	PHP_HTTP_ARG_VAL(method, 0)
PHP_HTTP_END_ARGS;

zend_function_entry php_http_request_method_method_entry[] = {
	PHP_HTTP_REQMETH_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_REQMETH_ME(__toString, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQMETH_ME(getId, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQMETH_ME(exists, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_HTTP_REQMETH_ME(register, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpRequestMethod, __construct)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		char *meth_str;
		int meth_len;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &meth_str, &meth_len)) {
			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(request_method)) {
				zval *zarg, *zret;

				if (SUCCESS == zend_get_parameters(ZEND_NUM_ARGS(), 1, &zarg)) {
					if (zend_call_method_with_1_params(&getThis(), php_http_request_method_class_entry, NULL, "exists", &zret, zarg)) {
						if (i_zend_is_true(zret)) {
							zend_update_property_stringl(php_http_request_method_class_entry, getThis(), ZEND_STRL("name"), meth_str, meth_len TSRMLS_CC);
						} else {
							php_http_error(HE_THROW, PHP_HTTP_E_REQUEST_METHOD, "Unknown request method '%s'", meth_str);
						}
						zval_ptr_dtor(&zret);
					}
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestMethod, __toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *retval = php_http_zsep(IS_STRING, zend_read_property(php_http_request_method_class_entry, getThis(), ZEND_STRL("name"), 0 TSRMLS_CC));

		RETURN_ZVAL(retval, 0, 0);
	}
	RETURN_EMPTY_STRING();
}

PHP_METHOD(HttpRequestMethod, getId)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval **data, *meth = php_http_zsep(IS_STRING, zend_read_property(php_http_request_method_class_entry, getThis(), ZEND_STRL("name"), 0 TSRMLS_CC));

		if (SUCCESS == zend_hash_find(&php_http_request_method_class_entry->constants_table, Z_STRVAL_P(meth), Z_STRLEN_P(meth) + 1, (void *) &data)) {
			zval_ptr_dtor(&meth);
			RETURN_ZVAL(*data, 1, 0);
		}
		zval_ptr_dtor(&meth);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestMethod, exists)
{
	char *meth_str;
	int meth_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &meth_str, &meth_len)) {
		zval **data;

		if (SUCCESS == zend_hash_find(&php_http_request_method_class_entry->constants_table, meth_str, meth_len + 1, (void *) &data)) {
			RETURN_ZVAL(*data, 1, 0);
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestMethod, register)
{
	char *meth_str;
	int meth_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &meth_str, &meth_len)) {
		RETURN_SUCCESS(zend_declare_class_constant_long(php_http_request_method_class_entry, meth_str, meth_len, zend_hash_num_elements(&php_http_request_class_entry->constants_table) TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_request_method)
{
	php_http_strlist_iterator_t std;

	PHP_HTTP_REGISTER_CLASS(http\\request, Method, http_request_method, php_http_object_class_entry, 0);

	zend_declare_property_null(php_http_request_method_class_entry, ZEND_STRL("name"), ZEND_ACC_PROTECTED TSRMLS_CC);

	php_http_strlist_iterator_init(&std, php_http_request_methods, 1);
	do {
		unsigned id;
		const char *meth = php_http_strlist_iterator_this(&std, &id);

		zend_declare_class_constant_long(php_http_request_method_class_entry, meth, strlen(meth), id TSRMLS_CC);
	} while (*php_http_strlist_iterator_next(&std));

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

