/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_std_defs.h"
#include "php_http_requestpool_object.h"
#include "php_http_request_pool_api.h"

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif
#include <curl/curl.h>

#define HTTP_BEGIN_ARGS(method, req_args) 		HTTP_BEGIN_ARGS_EX(HttpRequestPool, method, ZEND_RETURN_REFERENCE_AGNOSTIC, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)		HTTP_EMPTY_ARGS_EX(HttpRequestPool, method, ret_ref)
#define HTTP_REQPOOL_ME(method, visibility)		PHP_ME(HttpRequestPool, method, HTTP_ARGS(HttpRequestPool, method), visibility)

HTTP_BEGIN_ARGS_AR(HttpRequestPool, __construct, 0, 0)
	HTTP_ARG_OBJ(HttpRequest, request0, 0)
	HTTP_ARG_OBJ(HttpRequest, request1, 0)
	HTTP_ARG_OBJ(HttpRequest, requestN, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(__destruct, 0);
HTTP_EMPTY_ARGS(reset, 0);

HTTP_BEGIN_ARGS(attach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(detach, 1)
	HTTP_ARG_OBJ(HttpRequest, request, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(send, 0);
HTTP_EMPTY_ARGS(socketSend, 0);
HTTP_EMPTY_ARGS(socketSelect, 0);
HTTP_EMPTY_ARGS(socketRead, 0);


#define http_requestpool_object_declare_default_properties() _http_requestpool_object_declare_default_properties(TSRMLS_C)
static inline void _http_requestpool_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_requestpool_object_ce;
zend_function_entry http_requestpool_object_fe[] = {
	HTTP_REQPOOL_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_REQPOOL_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	HTTP_REQPOOL_ME(attach, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(detach, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(send, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(reset, ZEND_ACC_PUBLIC)

	HTTP_REQPOOL_ME(socketSend, ZEND_ACC_PROTECTED)
	HTTP_REQPOOL_ME(socketSelect, ZEND_ACC_PROTECTED)
	HTTP_REQPOOL_ME(socketRead, ZEND_ACC_PROTECTED)

	{NULL, NULL, NULL}
};
static zend_object_handlers http_requestpool_object_handlers;

void _http_requestpool_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpRequestPool, http_requestpool_object, NULL, 0);
}

zend_object_value _http_requestpool_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_requestpool_object *o;

	o = ecalloc(1, sizeof(http_requestpool_object));
	o->zo.ce = ce;

	http_request_pool_init(&o->pool);

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_requestpool_object_free, NULL TSRMLS_CC);
	ov.handlers = &http_requestpool_object_handlers;

	return ov;
}

static inline void _http_requestpool_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_requestpool_object_ce;

	DCL_PROP_N(PROTECTED, pool);
}

void _http_requestpool_object_free(zend_object *object TSRMLS_DC)
{
	http_requestpool_object *o = (http_requestpool_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	http_request_pool_dtor(&o->pool);
	efree(o);
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

