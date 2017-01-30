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

ZEND_RESULT_CODE php_http_header_parse(const char *header, size_t length, HashTable *headers, php_http_info_callback_t callback_func, void **callback_data)
{
	php_http_header_parser_t ctx;
	php_http_buffer_t buf;
	php_http_header_parser_state_t rs;
	
	if (!php_http_buffer_from_string_ex(&buf, header, length)) {
		php_error_docref(NULL, E_WARNING, "Could not allocate buffer");
		return FAILURE;
	}
	
	if (!php_http_header_parser_init(&ctx)) {
		php_http_buffer_dtor(&buf);
		php_error_docref(NULL, E_WARNING, "Could not initialize header parser");
		return FAILURE;
	}
	
	rs = php_http_header_parser_parse(&ctx, &buf, PHP_HTTP_HEADER_PARSER_CLEANUP, headers, callback_func, callback_data);
	php_http_header_parser_dtor(&ctx);
	php_http_buffer_dtor(&buf);

	return rs == PHP_HTTP_HEADER_PARSER_STATE_FAILURE ? FAILURE : SUCCESS;
}

void php_http_header_to_callback(HashTable *headers, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg)
{
	php_http_arrkey_t key;
	zval *header;

	ZEND_HASH_FOREACH_KEY_VAL(headers, key.h, key.key, header)
	{
		if (key.key) {
			php_http_header_to_callback_ex(key.key->val, header, crlf, cb, cb_arg);
		}
	}
	ZEND_HASH_FOREACH_END();
/*
<<<<<<< HEAD
	php_http_arrkey_t key;
	zval *header, *single_header;

	ZEND_HASH_FOREACH_KEY_VAL(headers, key.h, key.key, header)
	{
		if (key.key) {
			if (zend_string_equals_literal(key.key, "Set-Cookie") && Z_TYPE_P(header) == IS_ARRAY) {
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(header), single_header)
				{
					if (Z_TYPE_P(single_header) == IS_ARRAY) {
						php_http_cookie_list_t *cookie = php_http_cookie_list_from_struct(NULL, single_header);

						if (cookie) {
							char *buf;
							size_t len;

							php_http_cookie_list_to_string(cookie, &buf, &len);
							cb(cb_arg, crlf ? "Set-Cookie: %s" PHP_HTTP_CRLF : "Set-Cookie: %s", buf);
							php_http_cookie_list_free(&cookie);
							efree(buf);
						}
					} else {
						zend_string *zs = php_http_header_value_to_string(single_header);

						cb(cb_arg, crlf ? "Set-Cookie: %s" PHP_HTTP_CRLF : "Set-Cookie: %s", zs->val);
						zend_string_release(zs);
					}
				}
				ZEND_HASH_FOREACH_END();
			} else {
				zend_string *zs = php_http_header_value_to_string(header);

				cb(cb_arg, crlf ? "%s: %s" PHP_HTTP_CRLF : "%s: %s", key.key->val, zs->val);
				zend_string_release(zs);
			}
=======
>>>>>>> 343738ad56eb70017704fdac57cf0d74da3d0f2e
*/
}

void php_http_header_to_string(php_http_buffer_t *str, HashTable *headers)
{
	php_http_header_to_callback(headers, 1, (php_http_pass_format_callback_t) php_http_buffer_appendf, str);
}

void php_http_header_to_callback_ex(const char *key, zval *val, zend_bool crlf, php_http_pass_format_callback_t cb, void *cb_arg)
{
	zval *aval;
	zend_string *str;

	ZVAL_DEREF(val);
	switch (Z_TYPE_P(val)) {
	case IS_ARRAY:
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(val), aval)
		{
			php_http_header_to_callback_ex(key, aval, crlf, cb, cb_arg);
		}
		ZEND_HASH_FOREACH_END();
		break;

	case IS_TRUE:
		cb(cb_arg, "%s: true%s", key, crlf ? PHP_HTTP_CRLF:"");
		break;

	case IS_FALSE:
		cb(cb_arg, "%s: false%s", key, crlf ? PHP_HTTP_CRLF:"");
		break;

	default:
		str = zval_get_string(val);
		cb(cb_arg, "%s: %s%s", key, str->val, crlf ? PHP_HTTP_CRLF:"");
		zend_string_release(str);
		break;
	}
}

void php_http_header_to_string_ex(php_http_buffer_t *str, const char *key, zval *val)
{
	php_http_header_to_callback_ex(key, val, 1, (php_http_pass_format_callback_t) php_http_buffer_appendf, str);
}

zend_string *php_http_header_value_array_to_string(zval *header)
{
	zval *val;
	php_http_buffer_t str;

	php_http_buffer_init(&str);
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(header), val)
	{
		zend_string *zs = php_http_header_value_to_string(val);

		php_http_buffer_appendf(&str, str.used ? ", %s":"%s", zs->val);
		zend_string_release(zs);
	}
	ZEND_HASH_FOREACH_END();
	php_http_buffer_fix(&str);

	return php_http_cs2zs(str.data, str.used);
}

zend_string *php_http_header_value_to_string(zval *header)
{
	switch (Z_TYPE_P(header)) {
	case IS_TRUE:
		return zend_string_init(ZEND_STRL("true"), 0);
	case IS_FALSE:
		return zend_string_init(ZEND_STRL("false"), 0);
	case IS_ARRAY:
		return php_http_header_value_array_to_string(header);
	default:
		return zval_get_string(header);
	}
}

static zend_class_entry *php_http_header_class_entry;
zend_class_entry *php_http_header_get_class_entry(void)
{
	return php_http_header_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, __construct)
{
	char *name_str = NULL, *value_str = NULL;
	size_t name_len = 0, value_len = 0;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!s!", &name_str, &name_len, &value_str, &value_len), invalid_arg, return);

	if (name_str && name_len) {
		char *pretty_str = estrndup(name_str, name_len);
		zend_update_property_stringl(php_http_header_class_entry, getThis(), ZEND_STRL("name"), php_http_pretty_key(pretty_str, name_len, 1, 1), name_len);
		efree(pretty_str);
	}
	if (value_str && value_len) {
		zend_update_property_stringl(php_http_header_class_entry, getThis(), ZEND_STRL("value"), value_str, value_len);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_serialize, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, serialize)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_buffer_t buf;
		zend_string *zs;
		zval name_tmp, value_tmp;

		php_http_buffer_init(&buf);
		zs = zval_get_string(zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("name"), 0, &name_tmp));
		php_http_buffer_appendz(&buf, zs);
		zend_string_release(zs);

		zs = zval_get_string(zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0, &value_tmp));
		if (zs->len) {
			php_http_buffer_appends(&buf, ": ");
			php_http_buffer_appendz(&buf, zs);
		} else {
			php_http_buffer_appends(&buf, ":");
		}
		zend_string_release(zs);

		RETURN_STR(php_http_cs2zs(buf.data, buf.used));
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_unserialize, 0, 0, 1)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, unserialize)
{
	char *serialized_str;
	size_t serialized_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &serialized_str, &serialized_len)) {
		HashTable ht;

		zend_hash_init(&ht, 1, NULL, ZVAL_PTR_DTOR, 0);
		if (SUCCESS == php_http_header_parse(serialized_str, serialized_len, &ht, NULL, NULL)) {
			if (zend_hash_num_elements(&ht)) {
				zend_string *zs, *key;
				zend_ulong idx;

				zend_hash_internal_pointer_reset(&ht);
				switch (zend_hash_get_current_key(&ht, &key, &idx)) {
					case HASH_KEY_IS_STRING:
						zend_update_property_str(php_http_header_class_entry, getThis(), ZEND_STRL("name"), key);
						break;
					case HASH_KEY_IS_LONG:
						zend_update_property_long(php_http_header_class_entry, getThis(), ZEND_STRL("name"), idx);
						break;
					default:
						break;
				}
				zs = zval_get_string(zend_hash_get_current_data(&ht));
				zend_update_property_str(php_http_header_class_entry, getThis(), ZEND_STRL("value"), zs);
				zend_string_release(zs);
			}
		}
		zend_hash_destroy(&ht);
	}

}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_match, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, match)
{
	char *val_str = NULL;
	size_t val_len = 0;
	zend_long flags = PHP_HTTP_MATCH_LOOSE;
	zend_string *zs;
	zval value_tmp;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|sl", &val_str, &val_len, &flags)) {
		return;
	}

	zs = zval_get_string(zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0, &value_tmp));
	RETVAL_BOOL(php_http_match(zs->val, val_str, flags));
	zend_string_release(zs);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_negotiate, 0, 0, 1)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(1, result)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, negotiate)
{
	HashTable *supported, *rs;
	zval name_tmp, value_tmp, *rs_array = NULL;
	zend_string *zs;
	char *sep_str = NULL;
	size_t sep_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "H|z", &supported, &rs_array)) {
		return;
	}
	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}

	zs = zval_get_string(zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("name"), 0, &name_tmp));
	if (zend_string_equals_literal(zs, "Accept")) {
		sep_str = "/";
		sep_len = 1;
	} else if (zend_string_equals_literal(zs, "Accept-Language")) {
		sep_str = "-";
		sep_len = 1;
	}
	zend_string_release(zs);

	zs = zval_get_string(zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0, &value_tmp));
	if ((rs = php_http_negotiate(zs->val, zs->len, supported, sep_str, sep_len))) {
		PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(rs, supported, rs_array);
	} else {
		PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array);
	}
	zend_string_release(zs);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_getParams, 0, 0, 0)
	ZEND_ARG_INFO(0, param_sep)
	ZEND_ARG_INFO(0, arg_sep)
	ZEND_ARG_INFO(0, val_sep)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, getParams)
{
	zval value_tmp, zctor, zparams_obj, *zargs = NULL;
	
	ZVAL_STRINGL(&zctor, "__construct", lenof("__construct"));
	
	object_init_ex(&zparams_obj, php_http_params_get_class_entry());
	
	zargs = (zval *) ecalloc(ZEND_NUM_ARGS()+1, sizeof(zval));
	ZVAL_COPY_VALUE(&zargs[0], zend_read_property(php_http_header_class_entry, getThis(), ZEND_STRL("value"), 0, &value_tmp));
	if (ZEND_NUM_ARGS()) {
		zend_get_parameters_array(ZEND_NUM_ARGS(), ZEND_NUM_ARGS(), &zargs[1]);
	}
	
	if (SUCCESS == call_user_function(NULL, &zparams_obj, &zctor, return_value, ZEND_NUM_ARGS()+1, zargs)) {
		RETVAL_ZVAL(&zparams_obj, 0, 1);
	}
	
	zval_ptr_dtor(&zctor);
	if (zargs) {
		efree(zargs);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeader_parse, 0, 0, 1)
	ZEND_ARG_INFO(0, string)
	ZEND_ARG_INFO(0, header_class)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpHeader, parse)
{
	char *header_str;
	size_t header_len;
	zend_class_entry *ce = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|C", &header_str, &header_len, &ce)) {
		array_init(return_value);

		if (SUCCESS != php_http_header_parse(header_str, header_len, Z_ARRVAL_P(return_value), NULL, NULL)) {
			zval_dtor(return_value);
			RETURN_FALSE;
		} else {
			if (ce && instanceof_function(ce, php_http_header_class_entry)) {
				php_http_arrkey_t key;
				zval *val;

				ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(return_value), key.h, key.key, val)
				{
					zval zkey, zho;

					if (key.key) {
						ZVAL_STR_COPY(&zkey, key.key);
					} else {
						ZVAL_LONG(&zkey, key.h);
					}

					object_init_ex(&zho, ce);
					Z_TRY_ADDREF_P(val);
					zend_call_method_with_2_params(&zho, ce, NULL, "__construct", NULL, &zkey, val);
					zval_ptr_dtor(val);
					zval_ptr_dtor(&zkey);

					if (key.key) {
						add_assoc_zval_ex(return_value, key.key->val, key.key->len, &zho);
					} else {
						add_index_zval(return_value, key.h, &zho);
					}
				}
				ZEND_HASH_FOREACH_END();
			}
		}
	}
}

static zend_function_entry php_http_header_methods[] = {
	PHP_ME(HttpHeader, __construct,   ai_HttpHeader___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpHeader, serialize,     ai_HttpHeader_serialize, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpHeader, __toString, serialize, ai_HttpHeader_serialize, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpHeader, toString, serialize, ai_HttpHeader_serialize, ZEND_ACC_PUBLIC)
	PHP_ME(HttpHeader, unserialize,   ai_HttpHeader_unserialize, ZEND_ACC_PUBLIC)
	PHP_ME(HttpHeader, match,         ai_HttpHeader_match, ZEND_ACC_PUBLIC)
	PHP_ME(HttpHeader, negotiate,     ai_HttpHeader_negotiate, ZEND_ACC_PUBLIC)
	PHP_ME(HttpHeader, getParams,     ai_HttpHeader_getParams, ZEND_ACC_PUBLIC)
	PHP_ME(HttpHeader, parse,         ai_HttpHeader_parse, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_header)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Header", php_http_header_methods);
	php_http_header_class_entry = zend_register_internal_class(&ce);
	zend_class_implements(php_http_header_class_entry, 1, zend_ce_serializable);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_LOOSE"), PHP_HTTP_MATCH_LOOSE);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_CASE"), PHP_HTTP_MATCH_CASE);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_WORD"), PHP_HTTP_MATCH_WORD);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_FULL"), PHP_HTTP_MATCH_FULL);
	zend_declare_class_constant_long(php_http_header_class_entry, ZEND_STRL("MATCH_STRICT"), PHP_HTTP_MATCH_STRICT);
	zend_declare_property_null(php_http_header_class_entry, ZEND_STRL("name"), ZEND_ACC_PUBLIC);
	zend_declare_property_null(php_http_header_class_entry, ZEND_STRL("value"), ZEND_ACC_PUBLIC);

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

