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

PHP_HTTP_API STATUS php_http_new(zend_object_value *ov, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC)
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
	zeh = zend_read_static_property(php_http_object_get_class_entry(), ZEND_STRL("defaultErrorHandling"), 0 TSRMLS_CC);
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

PHP_HTTP_BEGIN_ARGS(setErrorHandling, 1)
	PHP_HTTP_ARG_VAL(eh, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getErrorHandling);

PHP_HTTP_BEGIN_ARGS(setDefaultErrorHandling, 1)
	PHP_HTTP_ARG_VAL(eh, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getDefaultErrorHandling);

PHP_HTTP_BEGIN_ARGS(triggerError, 3)
	PHP_HTTP_ARG_VAL(error_type, 0)
	PHP_HTTP_ARG_VAL(error_code, 0)
	PHP_HTTP_ARG_VAL(error_message, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_object_class_entry;

zend_class_entry *php_http_object_get_class_entry(void)
{
	return php_http_object_class_entry;
}

static zend_function_entry php_http_object_method_entry[] = {
	PHP_HTTP_OBJECT_ME(setErrorHandling, ZEND_ACC_PUBLIC)
	PHP_HTTP_OBJECT_ME(getErrorHandling, ZEND_ACC_PUBLIC)
	PHP_HTTP_OBJECT_ME(setDefaultErrorHandling, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_HTTP_OBJECT_ME(getDefaultErrorHandling, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_HTTP_OBJECT_ME(triggerError, ZEND_ACC_PUBLIC)

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
#if PHP_VERSION_ID < 50339
	zend_hash_copy(((zend_object *) o)->properties, &(ce->default_properties), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval*));
#else
	object_properties_init((zend_object *) o, ce);
#endif

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, (zend_objects_free_object_storage_t) zend_objects_free_object_storage, NULL TSRMLS_CC);
	ov.handlers = zend_get_std_object_handlers();

	return ov;
}

PHP_METHOD(HttpObject, getErrorHandling)
{
	RETURN_PROP(php_http_object_get_class_entry(), "errorHandling");
}

PHP_METHOD(HttpObject, setErrorHandling)
{
	long eh;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &eh)) {
		switch (eh) {
			case EH_NORMAL:
			case EH_SUPPRESS:
			case EH_THROW:
				zend_update_property_long(php_http_object_get_class_entry(), getThis(), ZEND_STRL("errorHandling"), eh TSRMLS_CC);
				break;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "unknown error handling code (%ld)", eh);
				break;
		}
	}

	RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpObject, getDefaultErrorHandling)
{
	RETURN_SPROP(php_http_object_get_class_entry(), "defaultErrorHandling");
}

PHP_METHOD(HttpObject, setDefaultErrorHandling)
{
	long eh;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &eh)) {
		switch (eh) {
			case EH_NORMAL:
			case EH_SUPPRESS:
			case EH_THROW:
				zend_update_static_property_long(php_http_object_get_class_entry(), ZEND_STRL("defaultErrorHandling"), eh TSRMLS_CC);
				break;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "unknown error handling code (%ld)", eh);
				break;
		}
	}
}

PHP_METHOD(HttpObject, triggerError)
{
	long eh, code;
	char *msg_str;
	int msg_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lls", &eh, &code, &msg_str, &msg_len)) {
		php_http_error(eh TSRMLS_CC, code, "%.*s", msg_len, msg_str);
	}
}

PHP_MINIT_FUNCTION(http_object)
{
	PHP_HTTP_REGISTER_CLASS(http, Object, http_object, NULL, ZEND_ACC_ABSTRACT);
	php_http_object_get_class_entry()->create_object = php_http_object_new;

	zend_declare_property_null(php_http_object_get_class_entry(), ZEND_STRL("defaultErrorHandling"), (ZEND_ACC_STATIC|ZEND_ACC_PROTECTED) TSRMLS_CC);
	zend_declare_property_null(php_http_object_get_class_entry(), ZEND_STRL("errorHandling"), ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_class_constant_long(php_http_object_get_class_entry(), ZEND_STRL("EH_NORMAL"), EH_NORMAL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_object_get_class_entry(), ZEND_STRL("EH_SUPPRESS"), EH_SUPPRESS TSRMLS_CC);
	zend_declare_class_constant_long(php_http_object_get_class_entry(), ZEND_STRL("EH_THROW"), EH_THROW TSRMLS_CC);

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

