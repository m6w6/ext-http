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

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	o->intern = intern;
	o->zo.handlers = &php_http_object_handlers;

	return o;
}

void php_http_object_free(zend_object *object)
{
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

php_http_object_method_t *php_http_object_method_init(php_http_object_method_t *cb, zval *zobject, const char *method_str, size_t method_len)
{
	if (!cb) {
		cb = ecalloc(1, sizeof(*cb));
	} else {
		memset(cb, 0, sizeof(*cb));
	}

	cb->fci.size = sizeof(cb->fci);
	ZVAL_STRINGL(&cb->fci.function_name, method_str, method_len);
#if PHP_VERSION_ID < 70300
	cb->fcc.initialized = 1;
#endif
	cb->fcc.calling_scope = cb->fcc.called_scope = Z_OBJCE_P(zobject);
	cb->fcc.function_handler = Z_OBJ_HT_P(zobject)->get_method(&Z_OBJ_P(zobject), Z_STR(cb->fci.function_name), NULL);

	return cb;
}

void php_http_object_method_dtor(php_http_object_method_t *cb)
{
	zval_ptr_dtor(&cb->fci.function_name);
}

void php_http_object_method_free(php_http_object_method_t **cb)
{
	if (*cb) {
		php_http_object_method_dtor(*cb);
		efree(*cb);
		*cb = NULL;
	}
}

ZEND_RESULT_CODE php_http_object_method_call(php_http_object_method_t *cb, zval *zobject, zval *retval_ptr, int argc, zval *args)
{
	ZEND_RESULT_CODE rv;
	zval retval;

	ZVAL_UNDEF(&retval);
	Z_ADDREF_P(zobject);
	cb->fci.object = Z_OBJ_P(zobject);
	cb->fcc.object = Z_OBJ_P(zobject);

	cb->fci.retval = retval_ptr ? retval_ptr : &retval;

	cb->fci.param_count = argc;
	cb->fci.params = args;

	if (cb->fcc.called_scope != Z_OBJCE_P(zobject)) {
		cb->fcc.called_scope = Z_OBJCE_P(zobject);
		cb->fcc.function_handler = Z_OBJ_HT_P(zobject)->get_method(&Z_OBJ_P(zobject), Z_STR(cb->fci.function_name), NULL);
	}

	rv = zend_call_function(&cb->fci, &cb->fcc);

	zval_ptr_dtor(zobject);
	if (!retval_ptr && !Z_ISUNDEF(retval)) {
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

