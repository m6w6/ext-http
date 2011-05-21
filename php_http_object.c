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

/* $Id: http_api.c 300299 2010-06-09 06:23:16Z mike $ */


#include "php_http.h"

STATUS php_http_new(zend_object_value *ov, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC)
{
	if (!ce) {
		ce = parent_ce;
	} else if (parent_ce && !instanceof_function(ce, parent_ce TSRMLS_CC)) {
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Class %s does not extend %s", ce->name, parent_ce->name);
		return FAILURE;
	}

	*ov = create(ce, intern_ptr, obj_ptr TSRMLS_CC);
	return SUCCESS;
}

PHP_HTTP_API zend_error_handling_t php_http_object_get_error_handling(zval *object TSRMLS_DC)
{
	zval *zeh, *lzeh;
	long eh;

	zeh = zend_read_property(Z_OBJCE_P(object), object, ZEND_STRL("errorHandling"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zeh) != IS_NULL) {
		lzeh = php_http_ztyp(IS_LONG, zeh);
		eh = Z_LVAL_P(lzeh);
		zval_ptr_dtor(&lzeh);
		return eh;
	}
	zeh = zend_read_static_property(php_http_object_class_entry, ZEND_STRL("defaultErrorHandling"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zeh) != IS_NULL) {
		lzeh = php_http_ztyp(IS_LONG, zeh);
		eh = Z_LVAL_P(lzeh);
		zval_ptr_dtor(&lzeh);
		return eh;
	}
	return EH_NORMAL;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpObject, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpObject, method, 0)
#define PHP_HTTP_OBJECT_ME(method, visibility)			PHP_ME(HttpObject, method, PHP_HTTP_ARGS(HttpObject, method), visibility)

PHP_HTTP_BEGIN_ARGS(factory, 1)
	PHP_HTTP_ARG_VAL(class_name, 0)
	PHP_HTTP_ARG_VAL(ctor_args, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setErrorHandling, 1)
	PHP_HTTP_ARG_VAL(eh, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getErrorHandling);

PHP_HTTP_BEGIN_ARGS(setDefaultErrorHandling, 1)
	PHP_HTTP_ARG_VAL(eh, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getDefaultErrorHandling);

zend_class_entry *php_http_object_class_entry;
zend_function_entry php_http_object_method_entry[] = {
	PHP_HTTP_OBJECT_ME(factory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	PHP_HTTP_OBJECT_ME(setErrorHandling, ZEND_ACC_PUBLIC)
	PHP_HTTP_OBJECT_ME(getErrorHandling, ZEND_ACC_PUBLIC)
	PHP_HTTP_OBJECT_ME(setDefaultErrorHandling, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_HTTP_OBJECT_ME(getDefaultErrorHandling, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	
	EMPTY_FUNCTION_ENTRY
};

zend_object_value php_http_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_object_new_ex(zend_class_entry *ce, void *nothing, php_http_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_object_t *o;

	o = ecalloc(1, sizeof(php_http_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, (zend_objects_free_object_storage_t) zend_objects_free_object_storage, NULL TSRMLS_CC);
	ov.handlers = zend_get_std_object_handlers();

	return ov;
}

PHP_METHOD(HttpObject, factory)
{
	zval *ctor_args = NULL;
	zend_class_entry *class_entry = NULL;
	zval *object_ctor;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "C|a/!", &class_entry, &ctor_args)) {
			object_init_ex(return_value, class_entry);

			MAKE_STD_ZVAL(object_ctor);
			array_init(object_ctor);

			Z_ADDREF_P(return_value);
			add_next_index_zval(object_ctor, return_value);
			add_next_index_stringl(object_ctor, ZEND_STRL("__construct"), 1);

			zend_fcall_info_init(object_ctor, 0, &fci, &fcc, NULL, NULL TSRMLS_CC);
			zend_fcall_info_call(&fci, &fcc, NULL, ctor_args TSRMLS_CC);

			zval_ptr_dtor(&object_ctor);
		}
	} end_error_handling();
}

PHP_METHOD(HttpObject, getErrorHandling)
{
	RETURN_PROP(php_http_object_class_entry, "errorHandling");
}

PHP_METHOD(HttpObject, setErrorHandling)
{
	long eh;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &eh)) {
		switch (eh) {
			case EH_NORMAL:
			case EH_SUPPRESS:
			case EH_THROW:
				zend_update_property_long(php_http_object_class_entry, getThis(), ZEND_STRL("errorHandling"), eh TSRMLS_CC);
				break;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "unknown error handling code (%ld)", eh);
		}
	}

	RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpObject, getDefaultErrorHandling)
{
	RETURN_SPROP(php_http_object_class_entry, "defaultErrorHandling");
}

PHP_METHOD(HttpObject, setDefaultErrorHandling)
{
	long eh;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &eh)) {
		switch (eh) {
			case EH_NORMAL:
			case EH_SUPPRESS:
			case EH_THROW:
				zend_update_static_property_long(php_http_object_class_entry, ZEND_STRL("defaultErrorHandling"), eh TSRMLS_CC);
				break;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "unknown error handling code (%ld)", eh);
		}
	}
}

PHP_MINIT_FUNCTION(http_object)
{
	PHP_HTTP_REGISTER_CLASS(http, Object, http_object, NULL, ZEND_ACC_ABSTRACT);
	php_http_object_class_entry->create_object = php_http_object_new;

	zend_declare_property_null(php_http_object_class_entry, ZEND_STRL("defaultErrorHandling"), (ZEND_ACC_STATIC|ZEND_ACC_PROTECTED) TSRMLS_CC);
	zend_declare_property_null(php_http_object_class_entry, ZEND_STRL("errorHandling"), ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_class_constant_long(php_http_object_class_entry, ZEND_STRL("EH_NORMAL"), EH_NORMAL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_object_class_entry, ZEND_STRL("EH_SUPPRESS"), EH_SUPPRESS TSRMLS_CC);
	zend_declare_class_constant_long(php_http_object_class_entry, ZEND_STRL("EH_THROW"), EH_THROW TSRMLS_CC);

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

