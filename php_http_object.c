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

static zend_object_handlers php_http_object_handlers;

zend_object *php_http_object_new(zend_class_entry *ce)
{
	return &php_http_object_new_ex(ce, NULL)->zo;
}

php_http_object_t *php_http_object_new_ex(zend_class_entry *ce, void *intern)
{
	php_http_object_t *o;

	o = ecalloc(1, sizeof(php_http_object_t) + (ce->default_properties_count - 1) * sizeof(zval));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	o->intern = intern;
	o->zo.handlers = &php_http_object_handlers;

	return o;
}

void php_http_object_free(zend_object *object)
{
	php_http_object_t *obj = PHP_HTTP_OBJ(object, NULL);
	zend_object_std_dtor(object);
}

ZEND_RESULT_CODE php_http_new(void **obj_ptr, zend_class_entry *ce, php_http_new_t create, zend_class_entry *parent_ce, void *intern_ptr)
{
	void *obj;

	if (!ce) {
		ce = parent_ce;
	} else if (parent_ce && !instanceof_function(ce, parent_ce)) {
		php_http_throw(unexpected_val, "Class %s does not extend %s", ce->name->val, parent_ce->name->val);
		return FAILURE;
	}

	obj = create(ce, intern_ptr);
	if (obj_ptr) {
		*obj_ptr = obj;
	}
	return SUCCESS;
}

ZEND_RESULT_CODE php_http_method_call(zval *object, const char *method_str, size_t method_len, int argc, zval argv[], zval *retval_ptr)
{
	zend_fcall_info fci;
	zval retval;
	ZEND_RESULT_CODE rv;

	ZVAL_UNDEF(&retval);
	fci.size = sizeof(fci);
	fci.object = Z_OBJ_P(object);
	fci.retval = retval_ptr ? retval_ptr : &retval;
	fci.param_count = argc;
	fci.params = argv;
	fci.no_separation = 1;
	fci.symbol_table = NULL;
	fci.function_table = NULL;

	ZVAL_STRINGL(&fci.function_name, method_str, method_len);
	rv = zend_call_function(&fci, NULL TSRMLS_CC);
	zval_ptr_dtor(&fci.function_name);

	if (!retval_ptr) {
		zval_ptr_dtor(&retval);
	}
	return rv;
}

PHP_MINIT_FUNCTION(http_object)
{
	memcpy(&php_http_object_handlers, zend_get_std_object_handlers(), sizeof(php_http_object_handlers));
	php_http_object_handlers.offset = XtOffsetOf(php_http_object_t, zo);

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

