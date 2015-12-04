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

static int grab_file(zval *tmp_name, int argc, va_list argv, zend_hash_key *key)
{
	zval *zfiles, *name, *zname, *error, *zerror, *type, *ztype, *size, *zsize;
	zend_hash_key *file_key;

	zfiles = (zval *) va_arg(argv, zval *);
	file_key = (zend_hash_key *) va_arg(argv, zend_hash_key *);
	name = (zval *) va_arg(argv, zval *);
	size = (zval *) va_arg(argv, zval *);
	type = (zval *) va_arg(argv, zval *);
	error = (zval *) va_arg(argv, zval *);

	if ((zname = zend_hash_index_find(Z_ARRVAL_P(name), key->h))
	&&	(zsize = zend_hash_index_find(Z_ARRVAL_P(size), key->h))
	&&	(ztype = zend_hash_index_find(Z_ARRVAL_P(type), key->h))
	&&	(zerror = zend_hash_index_find(Z_ARRVAL_P(error), key->h))
	) {
		zval entry, *array;

		array_init(&entry);

		Z_TRY_ADDREF_P(tmp_name);
		add_assoc_zval_ex(&entry, ZEND_STRL("file"), tmp_name);
		Z_TRY_ADDREF_P(zname);
		add_assoc_zval_ex(&entry, ZEND_STRL("name"), zname);
		Z_TRY_ADDREF_P(zsize);
		add_assoc_zval_ex(&entry, ZEND_STRL("size"), zsize);
		Z_TRY_ADDREF_P(ztype);
		add_assoc_zval_ex(&entry, ZEND_STRL("type"), ztype);
		Z_TRY_ADDREF_P(zerror);
		add_assoc_zval_ex(&entry, ZEND_STRL("error"), zerror);

		if (file_key->key && (array = zend_hash_find(Z_ARRVAL_P(zfiles), file_key->key))) {
			add_next_index_zval(array, &entry);
		} else if (!file_key->key && (array = zend_hash_index_find(Z_ARRVAL_P(zfiles), file_key->h))) {
			add_next_index_zval(array, &entry);
		} else {
			zval tmp;

			array_init(&tmp);
			add_next_index_zval(&tmp, &entry);
			if (file_key->key) {
				zend_hash_update(Z_ARRVAL_P(zfiles), file_key->key, &tmp);
			} else {
				zend_hash_index_update(Z_ARRVAL_P(zfiles), file_key->h, &tmp);
			}
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

static int grab_files(zval *val, int argc, va_list argv, zend_hash_key *key)
{
	zval *zfiles, *name, *tmp_name, *error, *type, *size;

	zfiles = (zval *) va_arg(argv, zval *);

	if ((Z_TYPE_P(val) == IS_ARRAY)
	&&	(tmp_name = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("tmp_name")))
	&&	(name = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("name")))
	&&	(size = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("size")))
	&&	(type = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("type")))
	&&	(error = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("error")))
	) {
		int count;

		if (Z_TYPE_P(tmp_name) == IS_ARRAY && (count = zend_hash_num_elements(Z_ARRVAL_P(tmp_name))) > 1) {
			if (count == zend_hash_num_elements(Z_ARRVAL_P(name))
			&&	count == zend_hash_num_elements(Z_ARRVAL_P(size))
			&&	count == zend_hash_num_elements(Z_ARRVAL_P(type))
			&&	count == zend_hash_num_elements(Z_ARRVAL_P(error))
			) {
				zend_hash_apply_with_arguments(Z_ARRVAL_P(tmp_name), grab_file, 6, zfiles, key, name, size, type, error);
			} else {
				/* wat?! */
				return ZEND_HASH_APPLY_STOP;
			}
		} else  {
			zval *tmp, entry;

			ZVAL_DUP(&entry, val);
			if ((tmp = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("tmp_name")))) {
				Z_ADDREF_P(tmp);
				add_assoc_zval_ex(&entry, ZEND_STRL("file"), tmp);
				zend_hash_str_del(Z_ARRVAL(entry), ZEND_STRL("tmp_name"));
			}
			if (key->key) {
				zend_hash_update(Z_ARRVAL_P(zfiles), key->key, &entry);
			} else {
				zend_hash_index_update(Z_ARRVAL_P(zfiles), key->h, &entry);
			}
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

#define PHP_HTTP_ENV_REQUEST_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init_env(NULL, PHP_HTTP_REQUEST); \
		} \
	} while(0)

static zend_class_entry *php_http_env_request_class_entry;
zend_class_entry *php_http_get_env_request_class_entry(void)
{
	return php_http_env_request_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest___construct, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, __construct)
{
	php_http_message_object_t *obj;
	zval *zsg, zqs;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	obj->body = NULL;

	php_http_expect(obj->message = php_http_message_init_env(obj->message, PHP_HTTP_REQUEST), unexpected_val, return);

	zsg = php_http_env_get_superglobal(ZEND_STRL("_GET"));
	object_init_ex(&zqs, php_http_querystring_get_class_entry());
	php_http_expect(SUCCESS == php_http_querystring_ctor(&zqs, zsg), unexpected_val, return);
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("query"), &zqs);
	zval_ptr_dtor(&zqs);

	zsg = php_http_env_get_superglobal(ZEND_STRL("_POST"));
	object_init_ex(&zqs, php_http_querystring_get_class_entry());
	php_http_expect(SUCCESS == php_http_querystring_ctor(&zqs, zsg), unexpected_val, return);
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("form"), &zqs);
	zval_ptr_dtor(&zqs);

	zsg = php_http_env_get_superglobal(ZEND_STRL("_COOKIE"));
	object_init_ex(&zqs, php_http_querystring_get_class_entry());
	php_http_expect(SUCCESS == php_http_querystring_ctor(&zqs, zsg), unexpected_val, return);
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("cookie"), &zqs);
	zval_ptr_dtor(&zqs);

	array_init(&zqs);
	if ((zsg = php_http_env_get_superglobal(ZEND_STRL("_FILES")))) {
		zend_hash_apply_with_arguments(Z_ARRVAL_P(zsg), grab_files, 1, &zqs);
	}
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("files"), &zqs);
	zval_ptr_dtor(&zqs);
}

#define call_querystring_get(prop) \
	do {\
		zend_fcall_info fci; \
		zend_fcall_info_cache fcc; \
		zval rv, mn, *args = ecalloc(sizeof(zval), ZEND_NUM_ARGS()); \
		zval *this_ptr = getThis(); \
		zval qs_tmp, *qs = zend_read_property(Z_OBJCE_P(this_ptr), this_ptr, ZEND_STRL(prop), 0, &qs_tmp); \
		 \
		ZVAL_NULL(&rv); \
		array_init(&mn); \
		Z_TRY_ADDREF_P(qs); \
		add_next_index_zval(&mn, qs); \
		add_next_index_stringl(&mn, ZEND_STRL("get")); \
		zend_fcall_info_init(&mn, 0, &fci, &fcc, NULL, NULL); \
		zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args); \
		zend_fcall_info_argp(&fci, ZEND_NUM_ARGS(), args); \
		zend_fcall_info_call(&fci, &fcc, &rv, NULL); \
		zend_fcall_info_args_clear(&fci, 1); \
		efree(args); \
		zval_dtor(&mn); \
		RETVAL_ZVAL(&rv, 0, 1); \
	} while(0);

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest_getForm, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, defval)
	ZEND_ARG_INFO(0, delete)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, getForm)
{
	if (ZEND_NUM_ARGS()) {
		call_querystring_get("form");
	} else {
		zval zform_tmp, *zform = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("form"), 0, &zform_tmp);
		RETURN_ZVAL(zform, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest_getQuery, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, defval)
	ZEND_ARG_INFO(0, delete)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, getQuery)
{
	if (ZEND_NUM_ARGS()) {
		call_querystring_get("query");
	} else {
		zval zquery_tmp, *zquery = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("query"), 0, &zquery_tmp);
		RETURN_ZVAL(zquery, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest_getCookie, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, defval)
	ZEND_ARG_INFO(0, delete)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, getCookie)
{
	if (ZEND_NUM_ARGS()) {
		call_querystring_get("cookie");
	} else {
		zval zcookie_tmp, *zcookie = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("cookie"), 0, &zcookie_tmp);
		RETURN_ZVAL(zcookie, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest_getFiles, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, getFiles)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval zfiles_tmp, *zfiles = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("files"), 0, &zfiles_tmp);
		RETURN_ZVAL(zfiles, 1, 0);
	}
}

static zend_function_entry php_http_env_request_methods[] = {
	PHP_ME(HttpEnvRequest, __construct,  ai_HttpEnvRequest___construct,  ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpEnvRequest, getForm,      ai_HttpEnvRequest_getForm,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvRequest, getQuery,     ai_HttpEnvRequest_getQuery,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvRequest, getCookie,    ai_HttpEnvRequest_getCookie,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvRequest, getFiles,     ai_HttpEnvRequest_getFiles,     ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_env_request)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Env", "Request", php_http_env_request_methods);
	php_http_env_request_class_entry = zend_register_internal_class_ex(&ce, php_http_message_get_class_entry());

	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("query"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("form"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("cookie"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("files"), ZEND_ACC_PROTECTED);

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
