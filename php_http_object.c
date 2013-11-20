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

STATUS php_http_method_call(zval *object, const char *method_str, size_t method_len, int argc, zval **argv[], zval **retval_ptr TSRMLS_DC)
{
	zend_fcall_info fci;
	zval zmethod;
	zval *retval;
	STATUS rv;

	fci.size = sizeof(fci);
	fci.object_ptr = object;
	fci.function_name = &zmethod;
	fci.retval_ptr_ptr = retval_ptr ? retval_ptr : &retval;
	fci.param_count = argc;
	fci.params = argv;
	fci.no_separation = 1;
	fci.symbol_table = NULL;
	fci.function_table = NULL;

	INIT_PZVAL(&zmethod);
	ZVAL_STRINGL(&zmethod, method_str, method_len, 0);
	rv = zend_call_function(&fci, NULL TSRMLS_CC);

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

