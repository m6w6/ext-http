/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_CURL
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "zend_interfaces.h"

#include "php_http_api.h"
#include "php_http_exception_object.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#include "php_http_request_datashare_api.h"
#include "php_http_requestdatashare_object.h"

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpRequestDataShare, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpRequestDataShare, method, 0)
#define HTTP_RSHARE_ME(method, visibility)	PHP_ME(HttpRequestDataShare, method, HTTP_ARGS(HttpRequestDataShare, method), visibility)

#if defined(HAVE_SPL) && !defined(WONKY)
/* SPL doesn't install its headers */
extern PHPAPI zend_class_entry *spl_ce_Countable;
#endif

HTTP_EMPTY_ARGS(__destruct);
HTTP_EMPTY_ARGS(count);

HTTP_BEGIN_ARGS(attach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;
HTTP_BEGIN_ARGS(detach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(reset);

HTTP_BEGIN_ARGS(factory, 0)
	HTTP_ARG_VAL(global, 0)
	HTTP_ARG_VAL(class_name, 0)
HTTP_END_ARGS;

#ifndef WONKY
HTTP_BEGIN_ARGS(singleton, 0)
	HTTP_ARG_VAL(global, 0)
HTTP_END_ARGS;
#endif


#define http_requestdatashare_object_read_prop _http_requestdatashare_object_read_prop
static zval *_http_requestdatashare_object_read_prop(zval *object, zval *member, int type TSRMLS_DC);
#define http_requestdatashare_object_write_prop _http_requestdatashare_object_write_prop
static void _http_requestdatashare_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC);
#define http_requestdatashare_instantiate(t, g) _http_requestdatashare_instantiate((t), (g) TSRMLS_CC)
static inline zval *_http_requestdatashare_instantiate(zval *this_ptr, zend_bool global TSRMLS_DC);

#define THIS_CE http_requestdatashare_object_ce
zend_class_entry *http_requestdatashare_object_ce;
zend_function_entry http_requestdatashare_object_fe[] = {
	HTTP_RSHARE_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	HTTP_RSHARE_ME(count, ZEND_ACC_PUBLIC)
	HTTP_RSHARE_ME(attach, ZEND_ACC_PUBLIC)
	HTTP_RSHARE_ME(detach, ZEND_ACC_PUBLIC)
	HTTP_RSHARE_ME(reset, ZEND_ACC_PUBLIC)
	HTTP_RSHARE_ME(factory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#ifndef WONKY
	HTTP_RSHARE_ME(singleton, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#endif
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_requestdatashare_object_handlers;

PHP_MINIT_FUNCTION(http_requestdatashare_object)
{
	HTTP_REGISTER_CLASS_EX(HttpRequestDataShare, http_requestdatashare_object, NULL, 0);
	http_requestdatashare_object_handlers.clone_obj = NULL;
	http_requestdatashare_object_handlers.read_property = http_requestdatashare_object_read_prop;
	http_requestdatashare_object_handlers.write_property = http_requestdatashare_object_write_prop;

#if defined(HAVE_SPL) && !defined(WONKY)
	zend_class_implements(http_requestdatashare_object_ce TSRMLS_CC, 1, spl_ce_Countable);
#endif
	
	zend_declare_property_null(THIS_CE, ZEND_STRS("instance")-1, (ZEND_ACC_STATIC|ZEND_ACC_PRIVATE) TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRS("cookie")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRS("dns")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRS("ssl")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_bool(THIS_CE, ZEND_STRS("connect")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	
	return SUCCESS;
}

zend_object_value _http_requestdatashare_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_requestdatashare_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_requestdatashare_object_new_ex(zend_class_entry *ce, http_request_datashare *share, http_requestdatashare_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_requestdatashare_object *o;

	o = ecalloc(1, sizeof(http_requestdatashare_object));
	o->zo.ce = ce;

	if (share) {
		o->share = share;
	} else {
		o->share = http_request_datashare_new();
	}
	
	if (ptr) {
		*ptr = o;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), zend_hash_num_elements(&ce->default_properties), NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_requestdatashare_object, o);
	ov.handlers = &http_requestdatashare_object_handlers;

	return ov;
}

void _http_requestdatashare_object_free(zend_object *object TSRMLS_DC)
{
	http_requestdatashare_object *o = (http_requestdatashare_object *) object;

	if (!o->share->persistent) {
		http_request_datashare_free(&o->share);
	}
	freeObject(o);
}

static zval *_http_requestdatashare_object_read_prop(zval *object, zval *member, int type TSRMLS_DC)
{
	if (type == BP_VAR_W && zend_hash_exists(&THIS_CE->default_properties, Z_STRVAL_P(member), Z_STRLEN_P(member)+1)) {
		zend_error(E_ERROR, "Cannot access HttpRequestDataShare default properties by reference or array key/index");
		return NULL;
	}
	
	return zend_get_std_object_handlers()->read_property(object, member, type TSRMLS_CC);
}

static void _http_requestdatashare_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC)
{
	if (zend_hash_exists(&THIS_CE->default_properties, Z_STRVAL_P(member), Z_STRLEN_P(member)+1)) {
		int status;
		zval *orig = value;
		getObjectEx(http_requestdatashare_object, obj, object);
		
		SEPARATE_ZVAL_IF_NOT_REF(&value);
		status = http_request_datashare_set(obj->share, Z_STRVAL_P(member), Z_STRLEN_P(member), (zend_bool) i_zend_is_true(value));
		if (orig != value) {
			zval_ptr_dtor(&value);
			value = orig;
		}
		if (SUCCESS != status) {
			return;
		}
	}
	
	zend_get_std_object_handlers()->write_property(object, member, value TSRMLS_CC);
}

/* {{{ proto void HttpRequestDataShare::__destruct()
	Clean up HttpRequestDataShare object. */
PHP_METHOD(HttpRequestDataShare, __destruct)
{
	NO_ARGS {
		getObject(http_requestdatashare_object, obj);
		http_request_datashare_detach_all(obj->share);
	}
}
/* }}} */

/* {{{ proto int HttpRequestDataShare::count()
	Implements Countable::count(). */
PHP_METHOD(HttpRequestDataShare, count)
{
	getObject(http_requestdatashare_object, obj);
	
	NO_ARGS;
	
	RETURN_LONG(zend_llist_count(HTTP_RSHARE_HANDLES(obj->share)));
}
/* }}} */

PHP_METHOD(HttpRequestDataShare, attach)
{
	zval *request;
	getObject(http_requestdatashare_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		RETURN_FALSE;
	}
	
	RETURN_SUCCESS(http_request_datashare_attach(obj->share, request));
}

PHP_METHOD(HttpRequestDataShare, detach)
{
	zval *request;
	getObject(http_requestdatashare_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		RETURN_FALSE;
	}
	
	RETURN_SUCCESS(http_request_datashare_detach(obj->share, request));
}

PHP_METHOD(HttpRequestDataShare, reset)
{
	NO_ARGS {
		getObject(http_requestdatashare_object, obj);
		http_request_datashare_detach_all(obj->share);
	}
}

PHP_METHOD(HttpRequestDataShare, factory)
{
	zend_bool global = 0;
	char *cn = NULL;
	int cl = 0;
	zend_object_value ov;
	
	SET_EH_THROW_HTTP();
	if (	SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bs", &global, &cn, &cl) &&
			SUCCESS == http_object_new(&ov, cn, cl, _http_requestdatashare_object_new_ex, http_requestdatashare_object_ce, NULL, NULL)) {
		RETVAL_OBJVAL(ov, 0);
		http_requestdatashare_instantiate(return_value, global);
	}
	SET_EH_NORMAL();
}

#ifndef WONKY
/* {{{ proto static HttpRequestDataShare HttpRequestDataShare::singleton([bool global = false])
	Get a single instance (differentiates between the global setting). */
PHP_METHOD(HttpRequestDataShare, singleton)
{
	zend_bool global = 0;
	zval *instance = *zend_std_get_static_property(THIS_CE, ZEND_STRS("instance")-1, 0 TSRMLS_CC);
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &global)) {
		zval **zobj_ptr = NULL, *zobj = NULL;
		
		if (Z_TYPE_P(instance) == IS_ARRAY) {
			if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(instance), global, (void *) &zobj_ptr)) {
				RETVAL_ZVAL(*zobj_ptr, 1, 0);
			} else {
				zobj = http_requestdatashare_instantiate(NULL, global);
				add_index_zval(instance, global, zobj);
				RETVAL_OBJECT(zobj, 1);
			}
		} else {
			MAKE_STD_ZVAL(instance);
			array_init(instance);
			
			zobj = http_requestdatashare_instantiate(NULL, global);
			add_index_zval(instance, global, zobj);
			RETVAL_OBJECT(zobj, 1);
			
			zend_update_static_property(THIS_CE, ZEND_STRS("instance")-1, instance TSRMLS_CC);
			zval_ptr_dtor(&instance);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */
#endif /* !WONKY */

static inline zval *_http_requestdatashare_instantiate(zval *this_ptr, zend_bool global TSRMLS_DC)
{
	if (!this_ptr) {
		MAKE_STD_ZVAL(this_ptr);
		Z_TYPE_P(this_ptr) = IS_OBJECT;
		this_ptr->value.obj = http_requestdatashare_object_new_ex(http_requestdatashare_object_ce, global ? http_request_datashare_global_get() : NULL, NULL);
	}
	if (global) {
		if (HTTP_G->request.datashare.cookie) {
			zend_update_property_bool(THIS_CE, getThis(), ZEND_STRS("cookie")-1, HTTP_G->request.datashare.cookie TSRMLS_CC);
		}
		if (HTTP_G->request.datashare.dns) {
			zend_update_property_bool(THIS_CE, getThis(), ZEND_STRS("dns")-1, HTTP_G->request.datashare.dns TSRMLS_CC);
		}
		if (HTTP_G->request.datashare.ssl) {
			zend_update_property_bool(THIS_CE, getThis(), ZEND_STRS("ssl")-1, HTTP_G->request.datashare.ssl TSRMLS_CC);
		}
		if (HTTP_G->request.datashare.connect) {
			zend_update_property_bool(THIS_CE, getThis(), ZEND_STRS("connect")-1, HTTP_G->request.datashare.connect TSRMLS_CC);
		}
	}
	return this_ptr;
}

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

