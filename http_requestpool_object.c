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
#include "php_http_request_pool_api.h"
#include "php_http_requestpool_object.h"

#if defined(HAVE_SPL) && !defined(WONKY)
/* SPL doesn't install its headers */
extern PHPAPI zend_class_entry *spl_ce_Countable;
#endif

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpRequestPool, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpRequestPool, method, 0)
#define HTTP_REQPOOL_ME(method, visibility)	PHP_ME(HttpRequestPool, method, HTTP_ARGS(HttpRequestPool, method), visibility)

HTTP_EMPTY_ARGS(__construct);

HTTP_EMPTY_ARGS(__destruct);
HTTP_EMPTY_ARGS(reset);

HTTP_BEGIN_ARGS(attach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(detach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(send);
HTTP_EMPTY_ARGS(socketPerform);
HTTP_EMPTY_ARGS(socketSelect);

HTTP_EMPTY_ARGS(valid);
HTTP_EMPTY_ARGS(current);
HTTP_EMPTY_ARGS(key);
HTTP_EMPTY_ARGS(next);
HTTP_EMPTY_ARGS(rewind);

HTTP_EMPTY_ARGS(count);

HTTP_EMPTY_ARGS(getAttachedRequests);
HTTP_EMPTY_ARGS(getFinishedRequests);

HTTP_BEGIN_ARGS(enablePipelining, 0)
	HTTP_ARG_VAL(enable, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(enableEvents, 0)
	HTTP_ARG_VAL(enable, 0)
HTTP_END_ARGS;

zend_class_entry *http_requestpool_object_ce;
zend_function_entry http_requestpool_object_fe[] = {
	HTTP_REQPOOL_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_REQPOOL_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	HTTP_REQPOOL_ME(attach, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(detach, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(send, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(reset, ZEND_ACC_PUBLIC)

	HTTP_REQPOOL_ME(socketPerform, ZEND_ACC_PROTECTED)
	HTTP_REQPOOL_ME(socketSelect, ZEND_ACC_PROTECTED)

	/* implements Iterator */
	HTTP_REQPOOL_ME(valid, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(current, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(key, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(next, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(rewind, ZEND_ACC_PUBLIC)
	
	/* implmenents Countable */
	HTTP_REQPOOL_ME(count, ZEND_ACC_PUBLIC)
	
	HTTP_REQPOOL_ME(getAttachedRequests, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(getFinishedRequests, ZEND_ACC_PUBLIC)
	
	HTTP_REQPOOL_ME(enablePipelining, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(enableEvents, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_requestpool_object_handlers;

PHP_MINIT_FUNCTION(http_requestpool_object)
{
	HTTP_REGISTER_CLASS_EX(HttpRequestPool, http_requestpool_object, NULL, 0);
	http_requestpool_object_handlers.clone_obj = NULL;
	
#if defined(HAVE_SPL) && !defined(WONKY)
	zend_class_implements(http_requestpool_object_ce TSRMLS_CC, 2, spl_ce_Countable, zend_ce_iterator);
#else
	zend_class_implements(http_requestpool_object_ce TSRMLS_CC, 1, zend_ce_iterator);
#endif
	
	return SUCCESS;
}

zend_object_value _http_requestpool_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_requestpool_object *o;

	o = ecalloc(1, sizeof(http_requestpool_object));
	o->zo.ce = ce;

	http_request_pool_init(&o->pool);

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), zend_hash_num_elements(&ce->default_properties), NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_requestpool_object, o);
	ov.handlers = &http_requestpool_object_handlers;

	return ov;
}

void _http_requestpool_object_free(zend_object *object TSRMLS_DC)
{
	http_requestpool_object *o = (http_requestpool_object *) object;

	http_request_pool_dtor(&o->pool);
	freeObject(o);
}

#define http_requestpool_object_llist2array _http_requestpool_object_llist2array
static void _http_requestpool_object_llist2array(zval **req, zval *array TSRMLS_DC)
{
	ZVAL_ADDREF(*req);
	add_next_index_zval(array, *req);
}

/* ### USERLAND ### */

/* {{{ proto void HttpRequestPool::__construct([HttpRequest request[, ...]])
	Creates a new HttpRequestPool object instance. */
PHP_METHOD(HttpRequestPool, __construct)
{
	int argc = ZEND_NUM_ARGS();
	zval ***argv = safe_emalloc(argc, sizeof(zval *), 0);
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_get_parameters_array_ex(argc, argv)) {
		int i;

		for (i = 0; i < argc; ++i) {
			if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), http_request_object_ce TSRMLS_CC)) {
				http_request_pool_attach(&obj->pool, *(argv[i]));
			}
		}
	}
	efree(argv);
	SET_EH_NORMAL();
	http_final(HTTP_EX_CE(request_pool));
}
/* }}} */

/* {{{ proto void HttpRequestPool::__destruct()
	Clean up HttpRequestPool object. */
PHP_METHOD(HttpRequestPool, __destruct)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	http_request_pool_detach_all(&obj->pool);
}
/* }}} */

/* {{{ proto void HttpRequestPool::reset()
	Detach all attached HttpRequest objects. */
PHP_METHOD(HttpRequestPool, reset)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;
	
	obj->iterator.pos = 0;
	http_request_pool_detach_all(&obj->pool);
}

/* {{{ proto bool HttpRequestPool::attach(HttpRequest request)
	Attach an HttpRequest object to this HttpRequestPool. WARNING: set all options prior attaching! */
PHP_METHOD(HttpRequestPool, attach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		if (obj->iterator.pos > 0 && obj->iterator.pos < zend_llist_count(&obj->pool.handles)) {
			http_error(HE_THROW, HTTP_E_REQUEST_POOL, "Cannot attach to the HttpRequestPool while the iterator is active");
		} else {
			status = http_request_pool_attach(&obj->pool, request);
		}
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::detach(HttpRequest request)
	Detach an HttpRequest object from this HttpRequestPool. */
PHP_METHOD(HttpRequestPool, detach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		obj->iterator.pos = -1;
		status = http_request_pool_detach(&obj->pool, request);
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::send()
	Send all attached HttpRequest objects in parallel. */
PHP_METHOD(HttpRequestPool, send)
{
	STATUS status;
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	SET_EH_THROW_HTTP();
	status = http_request_pool_send(&obj->pool);
	SET_EH_NORMAL();
	
	/* rethrow as HttpRequestPoolException */
	http_final(HTTP_EX_CE(request_pool));

	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketPerform()
	Returns TRUE until each request has finished its transaction. */
PHP_METHOD(HttpRequestPool, socketPerform)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	if (0 < http_request_pool_perform(&obj->pool)) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketSelect() */
PHP_METHOD(HttpRequestPool, socketSelect)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_request_pool_select(&obj->pool));
}
/* }}} */

/* {{{ proto bool HttpRequestPool::valid()
	Implements Iterator::valid(). */
PHP_METHOD(HttpRequestPool, valid)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_requestpool_object, obj);
		RETURN_BOOL(obj->iterator.pos >= 0 && obj->iterator.pos < zend_llist_count(&obj->pool.handles));
	}
}
/* }}} */

/* {{{ proto HttpRequest HttpRequestPool::current()
	Implements Iterator::current(). */
PHP_METHOD(HttpRequestPool, current)
{
	NO_ARGS;

	if (return_value_used) {
		long pos = 0;
		zval **current = NULL;
		zend_llist_position lpos;
		getObject(http_requestpool_object, obj);

		if (obj->iterator.pos < zend_llist_count(&obj->pool.handles)) {
			for (	current = zend_llist_get_first_ex(&obj->pool.handles, &lpos);
					current && obj->iterator.pos != pos++;
					current = zend_llist_get_next_ex(&obj->pool.handles, &lpos));
			if (current) {
				RETURN_OBJECT(*current, 1);
			}
		}
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto int HttpRequestPool::key()
	Implements Iterator::key(). */
PHP_METHOD(HttpRequestPool, key)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_requestpool_object, obj);
		RETURN_LONG(obj->iterator.pos);
	}
}
/* }}} */

/* {{{ proto void HttpRequestPool::next()
	Implements Iterator::next(). */
PHP_METHOD(HttpRequestPool, next)
{
	NO_ARGS {
		getObject(http_requestpool_object, obj);
		++(obj->iterator.pos);
	}
}
/* }}} */

/* {{{ proto void HttpRequestPool::rewind()
	Implements Iterator::rewind(). */
PHP_METHOD(HttpRequestPool, rewind)
{
	NO_ARGS {
		getObject(http_requestpool_object, obj);
		obj->iterator.pos = 0;
	}
}
/* }}} */

/* {{{ proto int HttpRequestPool::count()
	Implements Countable::count(). */
PHP_METHOD(HttpRequestPool, count)
{
	NO_ARGS {
		getObject(http_requestpool_object, obj);
		RETURN_LONG((long) zend_llist_count(&obj->pool.handles));
	}
}
/* }}} */

/* {{{ proto array HttpRequestPool::getAttachedRequests()
	Get attached HttpRequest objects. */
PHP_METHOD(HttpRequestPool, getAttachedRequests)
{
	getObject(http_requestpool_object, obj);
	
	NO_ARGS;
	
	array_init(return_value);
	zend_llist_apply_with_argument(&obj->pool.handles, 
		(llist_apply_with_arg_func_t) http_requestpool_object_llist2array, 
		return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto array HttpRequestPool::getFinishedRequests()
	Get attached HttpRequest objects that already have finished their work. */
PHP_METHOD(HttpRequestPool, getFinishedRequests)
{
	getObject(http_requestpool_object, obj);
	
	NO_ARGS;
		
	array_init(return_value);
	zend_llist_apply_with_argument(&obj->pool.finished,
		(llist_apply_with_arg_func_t) http_requestpool_object_llist2array,
		return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::enablePipelining([bool enable = true])
	Enables pipelining support for all attached requests if support in libcurl is given. */
PHP_METHOD(HttpRequestPool, enablePipelining)
{
	zend_bool enable = 1;
	getObject(http_requestpool_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		RETURN_FALSE;
	}
#if defined(HAVE_CURL_MULTI_SETOPT) && HTTP_CURL_VERSION(7,16,0)
	if (CURLM_OK == curl_multi_setopt(obj->pool.ch, CURLMOPT_PIPELINING, (long) enable)) {
		RETURN_TRUE;
	}
#endif
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool HttpRequestPool::enableEvents([bool enable = true])
	Enables event-driven I/O if support in libcurl is given. */
PHP_METHOD(HttpRequestPool, enableEvents)
{
	zend_bool enable = 1;
	getObject(http_requestpool_object, obj);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
#if defined(HTTP_HAVE_EVENT)
		obj->pool.useevents = enable;
		RETURN_TRUE;
#endif
	}
	RETURN_FALSE;
}
/* }}} */

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

