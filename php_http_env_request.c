/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

static int grab_file(void *zpp TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	zval *zfiles, **name, **zname, **error, **zerror, **type, **ztype, **size, **zsize, **tmp_name = zpp;
	zend_hash_key *file_key;

	zfiles = (zval *) va_arg(argv, zval *);
	file_key = (zend_hash_key *) va_arg(argv, zend_hash_key *);
	name = (zval **) va_arg(argv, zval **);
	size = (zval **) va_arg(argv, zval **);
	type = (zval **) va_arg(argv, zval **);
	error = (zval **) va_arg(argv, zval **);

	if (SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(name), key->h, (void *) &zname)
	&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(size), key->h, (void *) &zsize)
	&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(type), key->h, (void *) &ztype)
	&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(error), key->h, (void *) &zerror)
	) {
		zval *entry, **array;

		MAKE_STD_ZVAL(entry);
		array_init(entry);

		Z_ADDREF_PP(tmp_name);
		add_assoc_zval_ex(entry, ZEND_STRS("file"), *tmp_name);
		Z_ADDREF_PP(zname);
		add_assoc_zval_ex(entry, ZEND_STRS("name"), *zname);
		Z_ADDREF_PP(zsize);
		add_assoc_zval_ex(entry, ZEND_STRS("size"), *zsize);
		Z_ADDREF_PP(ztype);
		add_assoc_zval_ex(entry, ZEND_STRS("type"), *ztype);
		Z_ADDREF_PP(zerror);
		add_assoc_zval_ex(entry, ZEND_STRS("error"), *zerror);

		if (SUCCESS == zend_hash_quick_find(Z_ARRVAL_P(zfiles), file_key->arKey, file_key->nKeyLength, file_key->h, (void *) &array)) {
			add_next_index_zval(*array, entry);
		} else {
			zval *tmp;

			MAKE_STD_ZVAL(tmp);
			array_init(tmp);
			add_next_index_zval(tmp, entry);
			zend_hash_quick_update(Z_ARRVAL_P(zfiles), file_key->arKey, file_key->nKeyLength, file_key->h, (void *) &tmp, sizeof(zval *), NULL);
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

static int grab_files(void *zpp TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	zval *zfiles, **name, **tmp_name, **error, **type, **size, **val = zpp;

	zfiles = (zval *) va_arg(argv, zval *);

	if (Z_TYPE_PP(val) == IS_ARRAY
	&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("tmp_name"), (void *) &tmp_name)
	&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("name"), (void *) &name)
	&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("size"), (void *) &size)
	&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("type"), (void *) &type)
	&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("error"), (void *) &error)
	) {
		int count;

		if (Z_TYPE_PP(tmp_name) == IS_ARRAY && (count = zend_hash_num_elements(Z_ARRVAL_PP(tmp_name))) > 1) {
			if (count == zend_hash_num_elements(Z_ARRVAL_PP(name))
			&&	count == zend_hash_num_elements(Z_ARRVAL_PP(size))
			&&	count == zend_hash_num_elements(Z_ARRVAL_PP(type))
			&&	count == zend_hash_num_elements(Z_ARRVAL_PP(error))
			) {
				zend_hash_apply_with_arguments(Z_ARRVAL_PP(tmp_name) TSRMLS_CC, grab_file, 6, zfiles, key, name, size, type, error);
			} else {
				/* wat?! */
				return ZEND_HASH_APPLY_STOP;
			}
		} else  {
			zval *cpy, **tmp;

			MAKE_STD_ZVAL(cpy);
			MAKE_COPY_ZVAL(val, cpy);
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(cpy), ZEND_STRS("tmp_name"), (void *) &tmp)) {
				Z_ADDREF_PP(tmp);
				add_assoc_zval_ex(cpy, ZEND_STRS("file"), *tmp);
				zend_hash_del_key_or_index(Z_ARRVAL_P(cpy), ZEND_STRS("tmp_name"), 0, HASH_DEL_KEY);
			}
			zend_hash_quick_update(Z_ARRVAL_P(zfiles), key->arKey, key->nKeyLength, key->h, (void *) &cpy, sizeof(zval *), NULL);
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

#define PHP_HTTP_ENV_REQUEST_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init_env(NULL, PHP_HTTP_REQUEST TSRMLS_CC); \
		} \
	} while(0)


ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest___construct, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, __construct)
{
	php_http_message_object_t *obj;
	zval *zsg, *zqs;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	obj->body = NULL;

	php_http_expect(obj->message = php_http_message_init_env(obj->message, PHP_HTTP_REQUEST TSRMLS_CC), unexpected_val, return);

	zsg = php_http_env_get_superglobal(ZEND_STRL("_GET") TSRMLS_CC);
	MAKE_STD_ZVAL(zqs);
	object_init_ex(zqs, php_http_querystring_class_entry);
	php_http_expect(SUCCESS == php_http_querystring_ctor(zqs, zsg TSRMLS_CC), unexpected_val,
			zval_ptr_dtor(&zqs);
			return;
	);
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("query"), zqs TSRMLS_CC);
	zval_ptr_dtor(&zqs);

	zsg = php_http_env_get_superglobal(ZEND_STRL("_POST") TSRMLS_CC);
	MAKE_STD_ZVAL(zqs);
	object_init_ex(zqs, php_http_querystring_class_entry);
	php_http_expect(SUCCESS == php_http_querystring_ctor(zqs, zsg TSRMLS_CC), unexpected_val,
			zval_ptr_dtor(&zqs);
			return;
	);
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("form"), zqs TSRMLS_CC);
	zval_ptr_dtor(&zqs);

	MAKE_STD_ZVAL(zqs);
	array_init(zqs);
	if ((zsg = php_http_env_get_superglobal(ZEND_STRL("_FILES") TSRMLS_CC))) {
		zend_hash_apply_with_arguments(Z_ARRVAL_P(zsg) TSRMLS_CC, grab_files, 1, zqs);
	}
	zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("files"), zqs TSRMLS_CC);
	zval_ptr_dtor(&zqs);
}

#define call_querystring_get(prop) \
	do {\
		zend_fcall_info fci; \
		zend_fcall_info_cache fcc; \
		zval *rv, mn, ***args = ecalloc(sizeof(zval **), ZEND_NUM_ARGS()); \
		zval *qs = zend_read_property(Z_OBJCE_P(getThis()), getThis(), ZEND_STRL(prop), 0 TSRMLS_CC); \
		 \
		INIT_PZVAL(&mn); \
		array_init(&mn); \
		Z_ADDREF_P(qs); \
		add_next_index_zval(&mn, qs); \
		add_next_index_stringl(&mn, ZEND_STRL("get"), 1); \
		zend_fcall_info_init(&mn, 0, &fci, &fcc, NULL, NULL TSRMLS_CC); \
		zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args); \
		zend_fcall_info_argp(&fci TSRMLS_CC, ZEND_NUM_ARGS(), args); \
		zend_fcall_info_call(&fci, &fcc, &rv, NULL TSRMLS_CC); \
		zend_fcall_info_args_clear(&fci, 1); \
		efree(args); \
		zval_dtor(&mn); \
		RETVAL_ZVAL(rv, 0, 1); \
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
		zval *zform = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("form"), 0 TSRMLS_CC);
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
		zval *zquery = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("query"), 0 TSRMLS_CC);
		RETURN_ZVAL(zquery, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnvRequest_getFiles, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnvRequest, getFiles)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *zfiles = zend_read_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("files"), 0 TSRMLS_CC);
		RETURN_ZVAL(zfiles, 1, 0);
	}
}

static zend_function_entry php_http_env_request_methods[] = {
	PHP_ME(HttpEnvRequest, __construct,  ai_HttpEnvRequest___construct,  ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpEnvRequest, getForm,      ai_HttpEnvRequest_getForm,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvRequest, getQuery,     ai_HttpEnvRequest_getQuery,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpEnvRequest, getFiles,     ai_HttpEnvRequest_getFiles,     ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

zend_class_entry *php_http_env_request_class_entry;

PHP_MINIT_FUNCTION(http_env_request)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Env", "Request", php_http_env_request_methods);
	php_http_env_request_class_entry = zend_register_internal_class_ex(&ce, php_http_message_class_entry, NULL TSRMLS_CC);

	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("query"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("form"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_env_request_class_entry, ZEND_STRL("files"), ZEND_ACC_PROTECTED TSRMLS_CC);

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
