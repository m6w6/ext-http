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
#include "php_http_request_object.h"
#include "php_http_exception_object.h"

#include "zend_interfaces.h"

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif
#include <curl/curl.h>

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpRequestPool, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)	HTTP_EMPTY_ARGS_EX(HttpRequestPool, method, ret_ref)
#define HTTP_REQPOOL_ME(method, visibility)	PHP_ME(HttpRequestPool, method, HTTP_ARGS(HttpRequestPool, method), visibility)

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

HTTP_EMPTY_ARGS(valid, 0);
HTTP_EMPTY_ARGS(current, 1);
HTTP_EMPTY_ARGS(key, 0);
HTTP_EMPTY_ARGS(next, 0);
HTTP_EMPTY_ARGS(rewind, 0);

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

	/* implements Interator */
	HTTP_REQPOOL_ME(valid, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(current, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(key, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(next, ZEND_ACC_PUBLIC)
	HTTP_REQPOOL_ME(rewind, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
static zend_object_handlers http_requestpool_object_handlers;

void _http_requestpool_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpRequestPool, http_requestpool_object, NULL, 0);
	zend_class_implements(http_requestpool_object_ce TSRMLS_CC, 1, zend_ce_iterator);
}

zend_object_value _http_requestpool_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_requestpool_object *o;

	o = ecalloc(1, sizeof(http_requestpool_object));
	o->zo.ce = ce;

	http_request_pool_init(&o->pool);
	o->iterator.pos = 0;

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_requestpool_object, o);
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

/* ### USERLAND ### */

/* {{{ proto void HttpRequestPool::__construct([HttpRequest request[, ...]])
 *
 * Instantiate a new HttpRequestPool object.  An HttpRequestPool is
 * able to send several HttpRequests in parallel.
 *
 * Example:
 * <pre>
 * <?php
 * try {
 *     $pool = new HttpRequestPool(
 *         new HttpRequest('http://www.google.com/', HTTP_HEAD),
 *         new HttpRequest('http://www.php.net/', HTTP_HEAD)
 *     );
 *     $pool->send();
 *     foreach($pool as $request) {
 *         printf("%s is %s (%d)\n",
 *             $request->getResponseInfo('effective_url'),
 *             $request->getResponseCode() ? 'alive' : 'not alive',
 *             $request->getResponseCode()
 *         );
 *     }
 * } catch (HttpException $e) {
 *     echo $e;
 * }
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequestPool, __construct)
{
	int argc = ZEND_NUM_ARGS();
	zval ***argv = safe_emalloc(argc, sizeof(zval *), 0);
	getObject(http_requestpool_object, obj);

	if (SUCCESS == zend_get_parameters_array_ex(argc, argv)) {
		int i;

		for (i = 0; i < argc; ++i) {
			if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), http_request_object_ce TSRMLS_CC)) {
				http_request_pool_attach(&obj->pool, *(argv[i]));
			}
		}
	}
	efree(argv);
}
/* }}} */

/* {{{ proto void HttpRequestPool::__destruct()
 *
 * Clean up HttpRequestPool object.
 */
PHP_METHOD(HttpRequestPool, __destruct)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	http_request_pool_detach_all(&obj->pool);
}
/* }}} */

/* {{{ proto void HttpRequestPool::reset()
 *
 * Detach all attached HttpRequest objects.
 */
PHP_METHOD(HttpRequestPool, reset)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	http_request_pool_detach_all(&obj->pool);
}

/* {{{ proto bool HttpRequestPool::attach(HttpRequest request)
 *
 * Attach an HttpRequest object to this HttpRequestPool.
 * NOTE: set all options prior attaching!
 */
PHP_METHOD(HttpRequestPool, attach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		status = http_request_pool_attach(&obj->pool, request);
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::detach(HttpRequest request)
 *
 * Detach an HttpRequest object from this HttpRequestPool.
 */
PHP_METHOD(HttpRequestPool, detach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		status = http_request_pool_detach(&obj->pool, request);
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::send()
 *
 * Send all attached HttpRequest objects in parallel.
 */
PHP_METHOD(HttpRequestPool, send)
{
	STATUS status;
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	SET_EH_THROW_HTTP();
	status = http_request_pool_send(&obj->pool);
	SET_EH_NORMAL();

	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketSend()
 *
 * Usage:
 * <pre>
 * <?php
 *     while ($pool->socketSend()) {
 *         do_something_else();
 *         if (!$pool->socketSelect()) {
 *             die('Socket error');
 *         }
 *     }
 *     $pool->socketRead();
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequestPool, socketSend)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	RETURN_BOOL(0 < http_request_pool_perform(&obj->pool));
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketSelect()
 *
 * See HttpRequestPool::socketSend().
 */
PHP_METHOD(HttpRequestPool, socketSelect)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_request_pool_select(&obj->pool));
}
/* }}} */

/* {{{ proto protected void HttpRequestPool::socketRead()
 *
 * See HttpRequestPool::socketSend().
 */
PHP_METHOD(HttpRequestPool, socketRead)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	zend_llist_apply(&obj->pool.handles, (llist_apply_func_t) http_request_pool_responsehandler TSRMLS_CC);
}
/* }}} */

/* implements Iterator */

/* {{{ proto bool HttpRequestPool::valid()
 */
PHP_METHOD(HttpRequestPool, valid)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_requestpool_object, obj);
		RETURN_BOOL(obj->iterator.pos < zend_llist_count(&obj->pool.handles));
	}
}
/* }}} */

/* {{{ proto HttpRequest HttpRequestPool::current()
 */
PHP_METHOD(HttpRequestPool, current)
{
	NO_ARGS;

	IF_RETVAL_USED {
		long pos = 0;
		zval **current = NULL;
		zend_llist_position lpos;
		getObject(http_requestpool_object, obj);

		if (obj->iterator.pos < zend_llist_count(&obj->pool.handles)) {
			for (	current = zend_llist_get_first_ex(&obj->pool.handles, &lpos); 
					current && obj->iterator.pos != pos++; 
					current = zend_llist_get_next_ex(&obj->pool.handles, &lpos));
			if (current) {
				RETURN_OBJECT(*current);
			}
		}
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto long HttpRequestPool::key()
 */
PHP_METHOD(HttpRequestPool, key)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_requestpool_object, obj);
		RETURN_LONG(obj->iterator.pos);
	}
}
/* }}} */

/* {{{ proto void HttpRequestPool::next()
 */
PHP_METHOD(HttpRequestPool, next)
{
	NO_ARGS {
		getObject(http_requestpool_object, obj);
		++(obj->iterator.pos);
	}
}
/* }}} */

/* {{{ proto void HttpRequestPool::rewind()
 */
PHP_METHOD(HttpRequestPool, rewind)
{
	NO_ARGS {
		getObject(http_requestpool_object, obj);
		obj->iterator.pos = 0;
	}
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

