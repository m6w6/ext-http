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

zend_object_value php_http_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_object_new_ex(zend_class_entry *ce, void *nothing, php_http_object_t **ptr TSRMLS_DC)
{
	php_http_object_t *o;

	o = ecalloc(1, sizeof(php_http_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	o->zv.handle = zend_objects_store_put(o, NULL, (zend_objects_free_object_storage_t) zend_objects_free_object_storage, NULL TSRMLS_CC);
	o->zv.handlers = zend_get_std_object_handlers();

	return o->zv;
}

STATUS php_http_new(zend_object_value *ovp, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr, void **obj_ptr TSRMLS_DC)
{
	zend_object_value ov;

	if (!ce) {
		ce = parent_ce;
	} else if (parent_ce && !instanceof_function(ce, parent_ce TSRMLS_CC)) {
		php_http_throw(unexpected_val, "Class %s does not extend %s", ce->name, parent_ce->name);
		return FAILURE;
	}

	ov = create(ce, intern_ptr, obj_ptr TSRMLS_CC);
	if (ovp) {
		*ovp = ov;
	}
	return SUCCESS;
}

php_http_object_method_t *php_http_object_method_init(php_http_object_method_t *cb, zval *zobject, const char *method_str, size_t method_len TSRMLS_DC)
{
	zval *zfn;

	if (!cb) {
		cb = ecalloc(1, sizeof(*cb));
	} else {
		memset(cb, 0, sizeof(*cb));
	}

	MAKE_STD_ZVAL(zfn);
	ZVAL_STRINGL(zfn, method_str, method_len, 1);

	cb->fci.size = sizeof(cb->fci);
	cb->fci.function_name = zfn;
	cb->fcc.initialized = 1;
	cb->fcc.calling_scope = cb->fcc.called_scope = Z_OBJCE_P(zobject);
	cb->fcc.function_handler = Z_OBJ_HT_P(zobject)->get_method(&zobject, Z_STRVAL_P(cb->fci.function_name), Z_STRLEN_P(cb->fci.function_name), NULL TSRMLS_CC);

	return cb;
}

void php_http_object_method_dtor(php_http_object_method_t *cb)
{
	if (cb->fci.function_name) {
		zval_ptr_dtor(&cb->fci.function_name);
		cb->fci.function_name = NULL;
	}
}

void php_http_object_method_free(php_http_object_method_t **cb)
{
	if (*cb) {
		php_http_object_method_dtor(*cb);
		efree(*cb);
		*cb = NULL;
	}
}

STATUS php_http_object_method_call(php_http_object_method_t *cb, zval *zobject, zval **retval_ptr, int argc, zval ***args TSRMLS_DC)
{
	STATUS rv;
	zval *retval = NULL;

	Z_ADDREF_P(zobject);
	cb->fci.object_ptr = zobject;
	cb->fcc.object_ptr = zobject;

	cb->fci.retval_ptr_ptr = retval_ptr ? retval_ptr : &retval;

	cb->fci.param_count = argc;
	cb->fci.params = args;

	if (cb->fcc.called_scope != Z_OBJCE_P(zobject)) {
		cb->fcc.called_scope = Z_OBJCE_P(zobject);
		cb->fcc.function_handler = Z_OBJ_HT_P(zobject)->get_method(&zobject, Z_STRVAL_P(cb->fci.function_name), Z_STRLEN_P(cb->fci.function_name), NULL TSRMLS_CC);
	}

	rv = zend_call_function(&cb->fci, &cb->fcc TSRMLS_CC);

	zval_ptr_dtor(&zobject);
	if (!retval_ptr && retval) {
		zval_ptr_dtor(&retval);
	}

	return rv;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

