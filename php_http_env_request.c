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


#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 		PHP_HTTP_BEGIN_ARGS_EX(HttpEnvRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)					PHP_HTTP_EMPTY_ARGS_EX(HttpEnvRequest, method, 0)
#define PHP_HTTP_ENV_REQUEST_ME(method, visibility)	PHP_ME(HttpEnvRequest, method, PHP_HTTP_ARGS(HttpEnvRequest, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);
PHP_HTTP_EMPTY_ARGS(getForm);
PHP_HTTP_EMPTY_ARGS(getQuery);
PHP_HTTP_EMPTY_ARGS(getFiles);

static zend_class_entry *php_http_env_request_class_entry;

zend_class_entry *php_http_env_request_get_class_entry(void)
{
	return php_http_env_request_class_entry;
}

zend_function_entry php_http_env_request_method_entry[] = {
	PHP_HTTP_ENV_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_ENV_REQUEST_ME(getForm, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_REQUEST_ME(getQuery, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENV_REQUEST_ME(getFiles, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

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
		zval *entry;

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

		zend_hash_quick_update(Z_ARRVAL_P(zfiles), file_key->arKey, file_key->nKeyLength, file_key->h, (void *) &entry, sizeof(zval *), NULL);
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

PHP_METHOD(HttpEnvRequest, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			zval *zsg, *zqs;

			obj->message = php_http_message_init_env(obj->message, PHP_HTTP_REQUEST TSRMLS_CC);
			obj->body.handle = 0;

			zsg = php_http_env_get_superglobal(ZEND_STRL("_GET") TSRMLS_CC);
			MAKE_STD_ZVAL(zqs);
			object_init_ex(zqs, php_http_querystring_get_class_entry());
			if (SUCCESS == php_http_querystring_ctor(zqs, zsg TSRMLS_CC)) {
				zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("query"), zqs TSRMLS_CC);
			}
			zval_ptr_dtor(&zqs);

			zsg = php_http_env_get_superglobal(ZEND_STRL("_POST") TSRMLS_CC);
			MAKE_STD_ZVAL(zqs);
			object_init_ex(zqs, php_http_querystring_get_class_entry());
			if (SUCCESS == php_http_querystring_ctor(zqs, zsg TSRMLS_CC)) {
				zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("form"), zqs TSRMLS_CC);
			}
			zval_ptr_dtor(&zqs);
			
			MAKE_STD_ZVAL(zqs);
			array_init(zqs);
			if ((zsg = php_http_env_get_superglobal(ZEND_STRL("_FILES") TSRMLS_CC))) {
				zend_hash_apply_with_arguments(Z_ARRVAL_P(zsg) TSRMLS_CC, grab_files, 1, zqs);
			}

			zend_update_property(php_http_env_request_class_entry, getThis(), ZEND_STRL("files"), zqs TSRMLS_CC);
			zval_ptr_dtor(&zqs);
		}
	} end_error_handling();
}

PHP_METHOD(HttpEnvRequest, getForm)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_env_request_class_entry, "form");
	}
}

PHP_METHOD(HttpEnvRequest, getQuery)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_env_request_class_entry, "query");
	}
}

PHP_METHOD(HttpEnvRequest, getFiles)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_env_request_class_entry, "files");
	}
}


PHP_MINIT_FUNCTION(http_env_request)
{
	PHP_HTTP_REGISTER_CLASS(http\\Env, Request, http_env_request, php_http_message_get_class_entry(), 0);
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
