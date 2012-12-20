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

PHP_HTTP_API STATUS php_http_headers_parse(const char *header, size_t length, HashTable *headers, php_http_info_callback_t callback_func, void **callback_data TSRMLS_DC)
{
	php_http_header_parser_t ctx;
	php_http_buffer_t buf;
	php_http_header_parser_state_t rs;
	
	if (!php_http_buffer_from_string_ex(&buf, header, length)) {
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Could not allocate buffer");
		return FAILURE;
	}
	
	if (!php_http_header_parser_init(&ctx TSRMLS_CC)) {
		php_http_buffer_dtor(&buf);
		php_http_error(HE_WARNING, PHP_HTTP_E_HEADER, "Could not initialize header parser");
		return FAILURE;
	}
	
	rs = php_http_header_parser_parse(&ctx, &buf, PHP_HTTP_HEADER_PARSER_CLEANUP, headers, callback_func, callback_data);
	php_http_header_parser_dtor(&ctx);
	php_http_buffer_dtor(&buf);

	if (rs == PHP_HTTP_HEADER_PARSER_STATE_FAILURE) {
		php_http_error(HE_WARNING, PHP_HTTP_E_MALFORMED_HEADERS, "Could not parse headers");
		return FAILURE;
	}
	
	return SUCCESS;
}

PHP_HTTP_API void php_http_headers_to_callback(HashTable *headers, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg TSRMLS_DC)
{
	HashPosition pos1, pos2;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **header, **single_header;

	FOREACH_HASH_KEYVAL(pos1, headers, key, header) {
		if (key.type == HASH_KEY_IS_STRING) {
			if (key.len == sizeof("Set-Cookie") && !strcasecmp(key.str, "Set-Cookie") && Z_TYPE_PP(header) == IS_ARRAY) {
				FOREACH_VAL(pos2, *header, single_header) {
					if (Z_TYPE_PP(single_header) == IS_ARRAY) {
						php_http_cookie_list_t *cookie = php_http_cookie_list_from_struct(NULL, *single_header TSRMLS_CC);

						if (cookie) {
							char *buf;
							size_t len;

							php_http_cookie_list_to_string(cookie, &buf, &len);
							cb(cb_arg, crlf ? "Set-Cookie: %s" PHP_HTTP_CRLF : "Set-Cookie: %s", buf);
							php_http_cookie_list_free(&cookie);
							efree(buf);
						}
					} else {
						zval *strval = php_http_header_value_to_string(*single_header TSRMLS_CC);

						cb(cb_arg, crlf ? "Set-Cookie: %s" PHP_HTTP_CRLF : "Set-Cookie: %s", Z_STRVAL_P(strval));
						zval_ptr_dtor(&strval);
					}
				}
			} else {
				zval *strval = php_http_header_value_to_string(*header TSRMLS_CC);

				cb(cb_arg, crlf ? "%s: %s" PHP_HTTP_CRLF : "%s: %s", key.str, Z_STRVAL_P(strval));
				zval_ptr_dtor(&strval);
			}
		}
	}
}

PHP_HTTP_API void php_http_headers_to_string(php_http_buffer_t *str, HashTable *headers TSRMLS_DC)
{
	php_http_headers_to_callback(headers, 1, php_http_buffer_appendf, str TSRMLS_CC);
}

PHP_HTTP_API zval *php_http_header_value_to_string(zval *header TSRMLS_DC)
{
	zval *ret;

	if (Z_TYPE_P(header) == IS_BOOL) {
		MAKE_STD_ZVAL(ret);
		ZVAL_STRING(ret, Z_BVAL_P(header) ? "true" : "false", 1);
	} else if (Z_TYPE_P(header) == IS_ARRAY) {
		zval **val;
		HashPosition pos;
		php_http_buffer_t str;

		php_http_buffer_init(&str);
		MAKE_STD_ZVAL(ret);
		FOREACH_VAL(pos,header, val) {
			zval *strval = php_http_header_value_to_string(*val TSRMLS_CC);

			php_http_buffer_appendf(&str, str.used ? ", %s":"%s", Z_STRVAL_P(strval));
			zval_ptr_dtor(&strval);
		}
		php_http_buffer_fix(&str);
		ZVAL_STRINGL(ret, str.data, str.used, 0);
	} else  {
		ret = php_http_zsep(1, IS_STRING, header);
	}

	return ret;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpHeader, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpHeader, method, 0)
#define PHP_HTTP_HEADER_ME(method, v)			PHP_ME(HttpHeader, method, PHP_HTTP_ARGS(HttpHeader, method), v)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(name, 0)
	PHP_HTTP_ARG_VAL(value, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(serialize);
PHP_HTTP_BEGIN_ARGS(unserialize, 1)
	PHP_HTTP_ARG_VAL(serialized, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(match, 1)
	PHP_HTTP_ARG_VAL(value, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiate, 1)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(parse, 1)
	PHP_HTTP_ARG_VAL(string, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getParams, 0)
	PHP_HTTP_ARG_VAL(param_sep, 0)
	PHP_HTTP_ARG_VAL(arg_sep, 0)
	PHP_HTTP_ARG_VAL(val_sep, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_header_class_entry;

zend_class_entry *php_http_header_get_class_entry(void)
{
	return php_http_header_class_entry;
}

static zend_function_entry php_http_header_method_entry[] = {
	PHP_HTTP_HEADER_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_HEADER_ME(serialize, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpHeader, __toString, serialize, PHP_HTTP_ARGS(HttpHeader, serialize), ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpHeader, toString, serialize, PHP_HTTP_ARGS(HttpHeader, serialize), ZEND_ACC_PUBLIC)
	PHP_HTTP_HEADER_ME(unserialize, ZEND_ACC_PUBLIC)
	PHP_HTTP_HEADER_ME(match, ZEND_ACC_PUBLIC)
	PHP_HTTP_HEADER_ME(negotiate, ZEND_ACC_PUBLIC)
	PHP_HTTP_HEADER_ME(getParams, ZEND_ACC_PUBLIC)
	PHP_HTTP_HEADER_ME(parse, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpHeader, __construct)
{
	char *name_str = NULL, *value_str = NULL;
	int name_len = 0, value_len = 0;

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!", &name_str, &name_len, &value_str, &value_len)) {
			if (name_str && name_len) {
				char *pretty_str = estrndup(name_str, name_len);
				zend_update_property_stringl(php_http_header_class_entry, getThis(), ZEND_STRL("name"), php_http_pretty_key(pretty_str, name_len, 1, 1), name_len TSRMLS_CC);
				efree(pretty_str);
			}
			if (value_str && value_len) {
				zend_update_property_stringl(php_http_header_class_entry, getThis(), ZEND_STRL("value"), value_str, value_len TSRMLS_CC);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpHeader, serialize)
{
	php_http_buffer_t buf;
	zval *zname, *zvalue;

	php_http_buffer_init(&buf);
	zname = php_http_ztyp(IS_STRING, zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("name"), 0 TSRMLS_CC));
	php_http_buffer_append(&buf, Z_STRVAL_P(zname), Z_STRLEN_P(zname));
	zval_ptr_dtor(&zname);
	zvalue = php_http_ztyp(IS_STRING, zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0 TSRMLS_CC));
	if (Z_STRLEN_P(zvalue)) {
		php_http_buffer_appends(&buf, ": ");
		php_http_buffer_append(&buf, Z_STRVAL_P(zvalue), Z_STRLEN_P(zvalue));
	} else {
		php_http_buffer_appends(&buf, ":");
	}
	zval_ptr_dtor(&zvalue);

	RETURN_PHP_HTTP_BUFFER_VAL(&buf);
}

PHP_METHOD(HttpHeader, unserialize)
{
	char *serialized_str;
	int serialized_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &serialized_str, &serialized_len)) {
		HashTable ht;

		zend_hash_init(&ht, 1, NULL, ZVAL_PTR_DTOR, 0);
		if (SUCCESS == php_http_headers_parse(serialized_str, serialized_len, &ht, NULL, NULL TSRMLS_CC)) {
			if (zend_hash_num_elements(&ht)) {
				zval **val, *cpy;
				char *str;
				uint len;
				ulong idx;

				zend_hash_internal_pointer_reset(&ht);
				switch (zend_hash_get_current_key_ex(&ht, &str, &len, &idx, 0, NULL)) {
					case HASH_KEY_IS_STRING:
						zend_update_property_stringl(php_http_header_class_entry, getThis(), ZEND_STRL("name"), str, len - 1 TSRMLS_CC);
						break;
					case HASH_KEY_IS_LONG:
						zend_update_property_long(php_http_header_class_entry, getThis(), ZEND_STRL("name"), idx TSRMLS_CC);
						break;
					default:
						break;
				}
				zend_hash_get_current_data(&ht, (void *) &val);
				cpy = php_http_zsep(1, IS_STRING, *val);
				zend_update_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), cpy TSRMLS_CC);
				zval_ptr_dtor(&cpy);
			}
		}
		zend_hash_destroy(&ht);
	}

}

PHP_METHOD(HttpHeader, match)
{
	char *val_str;
	int val_len;
	long flags = 0;
	zval *zvalue;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl", &val_str, &val_len, &flags)) {
		RETURN_NULL();
	}

	zvalue = php_http_ztyp(IS_STRING, zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0 TSRMLS_CC));
	RETVAL_BOOL(php_http_match(Z_STRVAL_P(zvalue), val_str, flags));
	zval_ptr_dtor(&zvalue);
}

PHP_METHOD(HttpHeader, negotiate)
{
	HashTable *supported, *rs;
	zval *zname, *zvalue, *rs_array = NULL;
	char *sep_str = NULL;
	size_t sep_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}

		zname = php_http_ztyp(IS_STRING, zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("name"), 0 TSRMLS_CC));
		if (!strcasecmp(Z_STRVAL_P(zname), "Accept")) {
			sep_str = "/";
			sep_len = 1;
		} else if (!strcasecmp(Z_STRVAL_P(zname), "Accept-Language")) {
			sep_str = "-";
			sep_len = 1;
		}
		zval_ptr_dtor(&zname);

		zvalue = php_http_ztyp(IS_STRING, zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0 TSRMLS_CC));
		if ((rs = php_http_negotiate(Z_STRVAL_P(zvalue), Z_STRLEN_P(zvalue), supported, sep_str, sep_len TSRMLS_CC))) {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(rs, supported, rs_array);
		} else {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array);
		}
		zval_ptr_dtor(&zvalue);
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpHeader, getParams)
{
	zval zctor, *zparams_obj, **zargs = NULL;
	
	INIT_PZVAL(&zctor);
	ZVAL_STRINGL(&zctor, "__construct", lenof("__construct"), 0);
	
	MAKE_STD_ZVAL(zparams_obj);
	object_init_ex(zparams_obj, php_http_params_get_class_entry());
	
	zargs = (zval **) ecalloc(ZEND_NUM_ARGS()+1, sizeof(zval *));
	zargs[0] = zend_read_property(Z_OBJCE_P(getThis()), getThis(), ZEND_STRL("value"), 0 TSRMLS_CC);
	if (ZEND_NUM_ARGS()) {
		zend_get_parameters_array(ZEND_NUM_ARGS(), ZEND_NUM_ARGS(), &zargs[1]);
	}
	
	if (SUCCESS == call_user_function(NULL, &zparams_obj, &zctor, return_value, ZEND_NUM_ARGS()+1, zargs TSRMLS_CC)) {
		RETVAL_ZVAL(zparams_obj, 0, 1);
	}
	
	if (zargs) {
		efree(zargs);
	}
}

PHP_METHOD(HttpHeader, parse)
{
	char *header_str;
	int header_len;
	zend_class_entry *ce = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|C", &header_str, &header_len, &ce)) {
		array_init(return_value);

		if (SUCCESS != php_http_headers_parse(header_str, header_len, Z_ARRVAL_P(return_value), NULL, NULL TSRMLS_CC)) {
			php_http_error(HE_WARNING, PHP_HTTP_E_MALFORMED_HEADERS, "Could not parse headers");
			zval_dtor(return_value);
			RETVAL_NULL();
		} else {
			if (ce && instanceof_function(ce, php_http_header_class_entry TSRMLS_CC)) {
				HashPosition pos;
				php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
				zval **val;

				FOREACH_KEYVAL(pos, return_value, key, val) {
					zval *zho, *zkey, *zvalue;

					Z_ADDREF_PP(val);
					zvalue = *val;

					MAKE_STD_ZVAL(zkey);
					if (key.type == HASH_KEY_IS_LONG) {
						ZVAL_LONG(zkey, key.num);
					} else {
						ZVAL_STRINGL(zkey, key.str, key.len - 1, 1);
					}

					MAKE_STD_ZVAL(zho);
					object_init_ex(zho, ce);
					zend_call_method_with_2_params(&zho, ce, NULL, "__construct", NULL, zkey, zvalue);

					if (key.type == HASH_KEY_IS_LONG) {
						zend_hash_index_update(Z_ARRVAL_P(return_value), key.num, (void *) &zho, sizeof(zval *), NULL);
					} else {
						zend_hash_update(Z_ARRVAL_P(return_value), key.str, key.len, (void *) &zho, sizeof(zval *), NULL);
					}

					zval_ptr_dtor(&zvalue);
					zval_ptr_dtor(&zkey);
				}
			}
		}
	}
}

PHP_MINIT_FUNCTION(http_header)
{
	PHP_HTTP_REGISTER_CLASS(http, Header, http_header, php_http_object_get_class_entry(), 0);
	zend_class_implements(php_http_header_class_entry TSRMLS_CC, 1, zend_ce_serializable);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_LOOSE"), PHP_HTTP_MATCH_LOOSE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_CASE"), PHP_HTTP_MATCH_CASE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_WORD"), PHP_HTTP_MATCH_WORD TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_FULL"), PHP_HTTP_MATCH_FULL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_STRICT"), PHP_HTTP_MATCH_STRICT TSRMLS_CC);
	zend_declare_property_null(php_http_header_class_entry, ZEND_STRL("name"), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(php_http_header_class_entry, ZEND_STRL("value"), ZEND_ACC_PUBLIC TSRMLS_CC);

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

