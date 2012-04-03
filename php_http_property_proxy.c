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

#ifndef PHP_HTTP_PPDBG
#	define PHP_HTTP_PPDBG 0
#endif

PHP_HTTP_API php_http_property_proxy_t *php_http_property_proxy_init(php_http_property_proxy_t *proxy, zval *object, zval *member, zval *parent TSRMLS_DC)
{
	if (!proxy) {
		proxy = emalloc(sizeof(*proxy));
	}
	memset(proxy, 0, sizeof(*proxy));

	MAKE_STD_ZVAL(proxy->myself);
	ZVAL_OBJVAL(proxy->myself, php_http_property_proxy_object_new_ex(php_http_property_proxy_get_class_entry(), proxy, NULL TSRMLS_CC), 0);
	Z_ADDREF_P(object);
	proxy->object = object;
	proxy->member = php_http_ztyp(IS_STRING, member);
	if (parent) {
		Z_ADDREF_P(parent);
		proxy->parent = parent;
	}

#if PHP_HTTP_PPDBG
	fprintf(stderr, "proxy_init: %s\n", Z_STRVAL_P(proxy->member));
#endif
	return proxy;
}

PHP_HTTP_API void php_http_property_proxy_dtor(php_http_property_proxy_t *proxy)
{
	zval_ptr_dtor(&proxy->object);
	zval_ptr_dtor(&proxy->member);
	zval_ptr_dtor(&proxy->myself);
	if (proxy->parent) {
		zval_ptr_dtor(&proxy->parent);
	}
}

PHP_HTTP_API void php_http_property_proxy_free(php_http_property_proxy_t **proxy)
{
	if (*proxy) {
		php_http_property_proxy_dtor(*proxy);
		efree(*proxy);
		*proxy = NULL;
	}
}


#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpPropertyProxy, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpPropertyProxy, method, 0)
#define PHP_HTTP_PP_ME(method, visibility)				PHP_ME(HttpPropertyProxy, method, PHP_HTTP_ARGS(HttpPropertyProxy, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);

static zend_class_entry *php_http_property_proxy_class_entry;

zend_class_entry *php_http_property_proxy_get_class_entry(void)
{
	return php_http_property_proxy_class_entry;
}

static zend_function_entry php_http_property_proxy_method_entry[] = {
	PHP_HTTP_PP_ME(__construct, ZEND_ACC_FINAL|ZEND_ACC_PRIVATE)
	EMPTY_FUNCTION_ENTRY
};

static zend_object_handlers php_http_property_proxy_object_handlers;

zend_object_value php_http_property_proxy_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_property_proxy_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_property_proxy_object_new_ex(zend_class_entry *ce, php_http_property_proxy_t *proxy, php_http_property_proxy_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_property_proxy_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}
	o->proxy = proxy;

	ov.handle = zend_objects_store_put(o, NULL, php_http_property_proxy_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_property_proxy_object_handlers;

	return ov;
}

void php_http_property_proxy_object_free(void *object TSRMLS_DC)
{
	php_http_property_proxy_object_t *o = object;

	if (o->proxy) {
		php_http_property_proxy_free(&o->proxy);
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static zval *php_http_property_proxy_object_get(zval *object TSRMLS_DC)
{
	php_http_property_proxy_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);

#if PHP_HTTP_PPDBG
	fprintf(stderr, "proxy_get: %s\n", Z_STRVAL_P(obj->proxy->member));
#endif
	if (Z_TYPE_P(obj->proxy->object) == IS_OBJECT) {
		return zend_read_property(Z_OBJCE_P(obj->proxy->object), obj->proxy->object, Z_STRVAL_P(obj->proxy->member), Z_STRLEN_P(obj->proxy->member), 0 TSRMLS_CC);
	} else {
		zval **data = NULL;

		if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(obj->proxy->object), Z_STRVAL_P(obj->proxy->member), Z_STRLEN_P(obj->proxy->member)+1, (void *) &data)) {
			Z_ADDREF_PP(data);
		} else {
			zval *unset;

			MAKE_STD_ZVAL(unset);
			ZVAL_NULL(unset);
			zend_symtable_update(Z_ARRVAL_P(obj->proxy->object), Z_STRVAL_P(obj->proxy->member), Z_STRLEN_P(obj->proxy->member)+1, (void *) &unset, sizeof(zval *), (void *) &data);
		}

		return *data;
	}

	return NULL;
}

static void php_http_property_proxy_object_set(zval **object, zval *value TSRMLS_DC)
{
	php_http_property_proxy_object_t *obj = zend_object_store_get_object(*object TSRMLS_CC);
	zval *target = obj->proxy->parent ? php_http_property_proxy_object_get(obj->proxy->parent TSRMLS_CC) : obj->proxy->object;

#if PHP_HTTP_PPDBG
	fprintf(stderr, "proxy_set: %s\n", Z_STRVAL_P(obj->proxy->member));
#endif
	if (Z_TYPE_P(target) == IS_OBJECT) {
		zend_update_property(Z_OBJCE_P(target), target, Z_STRVAL_P(obj->proxy->member), Z_STRLEN_P(obj->proxy->member), value TSRMLS_CC);
	} else {
		Z_ADDREF_P(value);
		zend_symtable_update(Z_ARRVAL_P(target), Z_STRVAL_P(obj->proxy->member), Z_STRLEN_P(obj->proxy->member)+1, (void *) &value, sizeof(zval *), NULL);
	}
	/* propagate */
	if (obj->proxy->parent) {
		php_http_property_proxy_object_set(&obj->proxy->parent, target TSRMLS_CC);
		zval_ptr_dtor(&target);
	}
}

static STATUS php_http_property_proxy_object_cast(zval *object, zval *return_value, int type TSRMLS_DC)
{
	zval *old_value, *new_value;

	old_value = php_http_property_proxy_object_get(object TSRMLS_CC);
	new_value = php_http_ztyp(type, old_value);

	if (old_value != new_value) {
		zval_ptr_dtor(&old_value);
	}

	RETVAL_ZVAL(new_value, 0, 0);

	return SUCCESS;
}

static zval *php_http_property_proxy_object_read_dimension(zval *object, zval *offset, int type TSRMLS_DC)
{
	php_http_property_proxy_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	zval *retval = NULL, *property;

#if PHP_HTTP_PPDBG
	zval *ocpy = php_http_ztyp(IS_STRING, offset);
	fprintf(stderr, "read_dimension: %s.%s (%d)\n", Z_STRVAL_P(obj->proxy->member), Z_STRVAL_P(ocpy), type);
	zval_ptr_dtor(&ocpy);
#endif

	property = php_http_property_proxy_object_get(object TSRMLS_CC);
	if (type != BP_VAR_R) {
		Z_ADDREF_P(property);
		retval = php_http_property_proxy_init(NULL, property, offset, obj->proxy->myself TSRMLS_CC)->myself;
	} else if (Z_TYPE_P(property) == IS_ARRAY) {
		zval **data = NULL;

		if (Z_TYPE_P(offset) == IS_LONG) {
			if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(property), Z_LVAL_P(offset), (void *) &data))  {
				retval = *data;
			}
		} else {
			offset = php_http_ztyp(IS_STRING, offset);
			if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(property), Z_STRVAL_P(offset), Z_STRLEN_P(offset), (void *) &data)) {
				retval = *data;
			}
			zval_ptr_dtor(&offset);
		}

		if (data) {
			Z_ADDREF_PP(data);
		}
	}
	zval_ptr_dtor(&property);

	return retval;
}

static void php_http_property_proxy_object_write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC)
{
	zval *property = php_http_property_proxy_object_get(object TSRMLS_CC);

#if PHP_HTTP_PPDBG
	php_http_property_proxy_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	zval *ocpy = offset ? php_http_ztyp(IS_STRING, offset) : offset;
	fprintf(stderr, "write_dimension: %s.%s (%d)\n", Z_STRVAL_P(obj->proxy->member), ocpy?Z_STRVAL_P(ocpy):"", Z_TYPE_P(property));
	if(ocpy)zval_ptr_dtor(&ocpy);
#endif
	switch (Z_TYPE_P(property)) {
		case IS_NULL:
			array_init(property);
			/* no break */
		case IS_ARRAY:
			Z_ADDREF_P(value);
			if (!offset) {
				add_next_index_zval(property, value);
			} else if (Z_TYPE_P(offset) == IS_LONG) {
				add_index_zval(property, Z_LVAL_P(offset), value);
			} else {
				offset = php_http_ztyp(IS_STRING, offset);
				add_assoc_zval_ex(property, Z_STRVAL_P(offset), Z_STRLEN_P(offset) + 1, value);
				zval_ptr_dtor(&offset);
			}
			php_http_property_proxy_object_set(&object, property TSRMLS_CC);
			zval_ptr_dtor(&property);
			break;

		default:
			zval_ptr_dtor(&property);
			break;
	}
}

PHP_METHOD(HttpPropertyProxy, __construct)
{
}

PHP_MINIT_FUNCTION(http_property_proxy)
{
	PHP_HTTP_REGISTER_CLASS(http\\Object, PropertyProxy, http_property_proxy, NULL, ZEND_ACC_FINAL);
	php_http_property_proxy_class_entry->create_object = php_http_property_proxy_object_new;
	memcpy(&php_http_property_proxy_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_property_proxy_object_handlers.set = php_http_property_proxy_object_set;
	php_http_property_proxy_object_handlers.get = php_http_property_proxy_object_get;
	php_http_property_proxy_object_handlers.cast_object = php_http_property_proxy_object_cast;
	php_http_property_proxy_object_handlers.read_dimension = php_http_property_proxy_object_read_dimension;
	php_http_property_proxy_object_handlers.write_dimension = php_http_property_proxy_object_write_dimension;

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

