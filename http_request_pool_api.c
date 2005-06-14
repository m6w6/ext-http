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
#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_request_pool_api.h"
#include "php_http_request_object.h"
#include "php_http_requestpool_object.h"

#ifdef ZEND_ENGINE_2

#ifndef HTTP_DEBUG_REQPOOLS
#	define HTTP_DEBUG_REQPOOLS 0
#endif

static void http_request_pool_freebody(http_request_body **body);
static int http_request_pool_compare_handles(void *h1, void *h2);

/* {{{ http_request_pool *http_request_pool_init(http_request_pool *) */
PHP_HTTP_API http_request_pool *_http_request_pool_init(http_request_pool *pool TSRMLS_DC)
{
	zend_bool free_pool;
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initializing request pool\n");
#endif
	if ((free_pool = (!pool))) {
		pool = emalloc(sizeof(http_request_pool));
		pool->ch = NULL;
	}

	if (!pool->ch) {
		if (!(pool->ch = curl_multi_init())) {
			http_error(E_WARNING, HTTP_E_CURL, "Could not initialize curl");
			if (free_pool) {
				efree(pool);
			}
			return NULL;
		}
	}

	pool->unfinished = 0;
	zend_llist_init(&pool->handles, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&pool->bodies, sizeof(http_request_body *), (llist_dtor_func_t) http_request_pool_freebody, 0);
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initialized request pool %p\n", pool);
#endif
	return pool;
}
/* }}} */

/* {{{ STATUS http_request_pool_attach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_attach(http_request_pool *pool, zval *request TSRMLS_DC)
{
	getObjectEx(http_request_object, req, request);
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attaching request %p to pool %p\n", req, pool);
#endif
	if (req->pool) {
		http_error(E_WARNING, HTTP_E_CURL, "HttpRequest object is already member of an HttpRequestPool");
	} else {
		CURLMcode code;
		http_request_body *body = http_request_body_new();
		zval *info = GET_PROP_EX(req, request, responseInfo);

		if (SUCCESS != http_request_object_requesthandler(req, request, body)) {
			http_error_ex(E_WARNING, HTTP_E_CURL, "Could not initialize HttpRequest object for attaching to the HttpRequestPool");
		} else {
			code = curl_multi_add_handle(pool->ch, req->ch);
			if ((CURLM_OK != code) && (CURLM_CALL_MULTI_PERFORM != code)) {
				http_error_ex(E_WARNING, HTTP_E_CURL, "Could not attach HttpRequest object to the HttpRequestPool: %s", curl_multi_strerror(code));
			} else {
				req->pool = pool;
				zend_llist_add_element(&pool->handles, &request);
				zend_llist_add_element(&pool->bodies, &body);
				zval_add_ref(&request);
				return SUCCESS;
			}
		}
		efree(body);
	}
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_request_pool_detach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_detach(http_request_pool *pool, zval *request TSRMLS_DC)
{
	getObjectEx(http_request_object, req, request);
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching request %p (pool: %p) from pool %p\n", req, req->pool, pool);
#endif
	if (req->pool != pool) {
		http_error(E_WARNING, HTTP_E_CURL, "HttpRequest object is not attached to this HttpRequestPool");
	} else {
		CURLMcode code;

		if (CURLM_OK != (code = curl_multi_remove_handle(pool->ch, req->ch))) {
			http_error_ex(E_WARNING, HTTP_E_CURL, "Could not detach HttpRequest object from the HttpRequestPool: %s", curl_multi_strerror(code));
		} else {
			req->pool = NULL;
			zend_llist_del_element(&pool->handles, request, http_request_pool_compare_handles);
			return SUCCESS;
		}
	}
	return FAILURE;
}
/* }}} */

/* {{{ void http_request_pool_detach_all(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_detach_all(http_request_pool *pool TSRMLS_DC)
{
	int count = zend_llist_count(&pool->handles);
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching %d requests from pool %p\n", count, pool);
#endif
	/*
	 * we cannot apply a function to the llist which actually detaches 
	 * the curl handle *and* removes the llist element --
	 * so let's get our hands dirty
	 */
	if (count) {
		int i = 0;
		zend_llist_position pos;
		zval **handle, **handles = emalloc(count * sizeof(zval *));
		
		for (handle = zend_llist_get_first_ex(&pool->handles, &pos); handle; handle = zend_llist_get_next_ex(&pool->handles, &pos)) {
			handles[i++] = *handle;
		}
		for (i = 0; i < count; ++i) {
			http_request_pool_detach(pool, handles[i]);
		}
	}
}


/* {{{ STATUS http_request_pool_send(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_send(http_request_pool *pool TSRMLS_DC)
{
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attempt to send requests of pool %p\n", pool);
#endif
	while (http_request_pool_perform(pool)) {
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "%d unfinished requests of pool %p remaining\n", pool->unfinished, pool);
#endif
		if (SUCCESS != http_request_pool_select(pool)) {
			http_error(E_WARNING, HTTP_E_CURL, "Socket error");
			return FAILURE;
		}
	}
	zend_llist_apply(&pool->handles, (llist_apply_func_t) http_request_pool_responsehandler TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_pool_dtor(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool TSRMLS_DC)
{
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destructing request pool %p\n", pool);
#endif
	pool->unfinished = 0;
	zend_llist_clean(&pool->handles);
	zend_llist_clean(&pool->bodies);
	curl_multi_cleanup(pool->ch);
}
/* }}} */

/* {{{ STATUS http_request_pool_select(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_select(http_request_pool *pool)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout = {1, 0};

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	curl_multi_fdset(pool->ch, &R, &W, &E, &MAX);
	return (-1 != select(MAX + 1, &R, &W, &E, &timeout)) ? SUCCESS : FAILURE;
}
/* }}} */

/* {{{ int http_request_pool_perform(http_request_pool *) */
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool)
{
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(pool->ch, &pool->unfinished));
	return pool->unfinished;
}
/* }}} */

/* {{{ void http_request_pool_responsehandler(zval **) */
void _http_request_pool_responsehandler(zval **req TSRMLS_DC)
{
	getObjectEx(http_request_object, obj, *req);
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Fetching data from request %p of pool %p\n", obj, obj->pool);
#endif
	http_request_object_responsehandler(obj, *req);
}
/* }}} */

/*#*/

/* {{{ static void http_request_pool_freebody(http_request_body **) */
static void http_request_pool_freebody(http_request_body **body)
{
	TSRMLS_FETCH();
	http_request_body_free(*body);
}
/* }}} */

/* {{{ static int http_request_pool_compare_handles(void *, void *) */
static int http_request_pool_compare_handles(void *h1, void *h2)
{
	return ((*((zval **) h1)) == ((zval *) h2));
}
/* }}} */

#endif /* ZEND_ENGINE_2 */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

