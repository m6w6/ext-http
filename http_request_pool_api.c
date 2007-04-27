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
#define HTTP_WANT_EVENT
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_api.h"
#include "php_http_exception_object.h"
#include "php_http_persistent_handle_api.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#include "php_http_request_pool_api.h"
#include "php_http_requestpool_object.h"

#ifndef HTTP_DEBUG_REQPOOLS
#	define HTTP_DEBUG_REQPOOLS 0
#endif

#ifdef HTTP_HAVE_EVENT
typedef struct _http_request_pool_event_t {
	struct event evnt;
	http_request_pool *pool;
} http_request_pool_event;

static int http_request_pool_socket_callback(CURL *easy, curl_socket_t s, int action, void *, void *);
#endif

static int http_request_pool_compare_handles(void *h1, void *h2);

PHP_MINIT_FUNCTION(http_request_pool)
{
	if (SUCCESS != http_persistent_handle_provide("http_request_pool", curl_multi_init, (http_persistent_handle_dtor) curl_multi_cleanup, NULL)) {
		return FAILURE;
	}
	return SUCCESS;
}

#ifdef HTTP_HAVE_EVENT
PHP_RINIT_FUNCTION(http_request_pool)
{
	if (!HTTP_G->request.pool.event.base && !(HTTP_G->request.pool.event.base = event_init())) {
		return FAILURE;
	}
	
	return SUCCESS;
}
#endif

/* {{{ http_request_pool *http_request_pool_init(http_request_pool *) */
PHP_HTTP_API http_request_pool *_http_request_pool_init(http_request_pool *pool TSRMLS_DC)
{
	zend_bool free_pool;
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initializing request pool %p\n", pool);
#endif
	
	if ((free_pool = (!pool))) {
		pool = emalloc(sizeof(http_request_pool));
		pool->ch = NULL;
	}

	if (SUCCESS != http_persistent_handle_acquire("http_request_pool", &pool->ch)) {
		if (free_pool) {
			efree(pool);
		}
		return NULL;
	}
	
	TSRMLS_SET_CTX(pool->tsrm_ls);
	
#if HTTP_HAVE_EVENT
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETDATA, pool);
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETFUNCTION, http_request_pool_socket_callback);
#endif
	
	pool->unfinished = 0;
	zend_llist_init(&pool->finished, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&pool->handles, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initialized request pool %p\n", pool);
#endif
	
	return pool;
}
/* }}} */

/* {{{ STATUS http_request_pool_attach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_attach(http_request_pool *pool, zval *request)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	getObjectEx(http_request_object, req, request);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attaching HttpRequest(#%d) %p to pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif
	
	if (req->pool) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is already member of %s HttpRequestPool", Z_OBJ_HANDLE_P(request), req->pool == pool ? "this" : "another");
	} else if (SUCCESS != http_request_object_requesthandler(req, request)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not initialize HttpRequest object(#%d) for attaching to the HttpRequestPool", Z_OBJ_HANDLE_P(request));
	} else {
		CURLMcode code = curl_multi_add_handle(pool->ch, req->request->ch);

		if ((CURLM_OK != code) && (CURLM_CALL_MULTI_PERFORM != code)) {
			http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "Could not attach HttpRequest object(#%d) to the HttpRequestPool: %s", Z_OBJ_HANDLE_P(request), curl_multi_strerror(code));
		} else {
			req->pool = pool;

			ZVAL_ADDREF(request);
			zend_llist_add_element(&pool->handles, &request);

#if HTTP_DEBUG_REQPOOLS
			fprintf(stderr, "> %d HttpRequests attached to pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
			return SUCCESS;
		}
	}
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_request_pool_detach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_detach(http_request_pool *pool, zval *request)
{
	CURLMcode code;
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	getObjectEx(http_request_object, req, request);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching HttpRequest(#%d) %p from pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif
	
	if (!req->pool) {
		/* not attached to any pool */
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "HttpRequest object(#%d) %p is not attached to any HttpRequestPool\n", Z_OBJ_HANDLE_P(request), req);
#endif
	} else if (req->pool != pool) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is not attached to this HttpRequestPool", Z_OBJ_HANDLE_P(request));
	} else if (req->request->_in_progress_cb) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "HttpRequest object(#%d) cannot be detached from the HttpRequestPool while executing the progress callback", Z_OBJ_HANDLE_P(request));
	} else if (CURLM_OK != (code = curl_multi_remove_handle(pool->ch, req->request->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "Could not detach HttpRequest object(#%d) from the HttpRequestPool: %s", Z_OBJ_HANDLE_P(request), curl_multi_strerror(code));
	} else {
		req->pool = NULL;
		zend_llist_del_element(&pool->finished, request, http_request_pool_compare_handles);
		zend_llist_del_element(&pool->handles, request, http_request_pool_compare_handles);
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "> %d HttpRequests remaining in pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ void http_request_pool_apply(http_request_pool *, http_request_pool_apply_func) */
PHP_HTTP_API void _http_request_pool_apply(http_request_pool *pool, http_request_pool_apply_func cb)
{
	int count = zend_llist_count(&pool->handles);
	
	if (count) {
		int i = 0;
		zend_llist_position pos;
		zval **handle, **handles = emalloc(count * sizeof(zval *));

		for (handle = zend_llist_get_first_ex(&pool->handles, &pos); handle; handle = zend_llist_get_next_ex(&pool->handles, &pos)) {
			handles[i++] = *handle;
		}
		
		/* should never happen */
		if (i != count) {
			zend_error(E_ERROR, "number of fetched request handles do not match overall count");
			count = i;
		}
		
		for (i = 0; i < count; ++i) {
			if (cb(pool, handles[i])) {
				break;
			}
		}
		efree(handles);
	}
}
/* }}} */

/* {{{ void http_request_pool_apply_with_arg(http_request_pool *, http_request_pool_apply_with_arg_func, void *) */
PHP_HTTP_API void _http_request_pool_apply_with_arg(http_request_pool *pool, http_request_pool_apply_with_arg_func cb, void *arg)
{
	int count = zend_llist_count(&pool->handles);
	
	if (count) {
		int i = 0;
		zend_llist_position pos;
		zval **handle, **handles = emalloc(count * sizeof(zval *));

		for (handle = zend_llist_get_first_ex(&pool->handles, &pos); handle; handle = zend_llist_get_next_ex(&pool->handles, &pos)) {
			handles[i++] = *handle;
		}
		
		/* should never happen */
		if (i != count) {
			zend_error(E_ERROR, "number of fetched request handles do not match overall count");
			count = i;
		}
		
		for (i = 0; i < count; ++i) {
			if (cb(pool, handles[i], arg)) {
				break;
			}
		}
		efree(handles);
	}
}
/* }}} */

/* {{{ void http_request_pool_detach_all(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_detach_all(http_request_pool *pool)
{
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching %d requests from pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	http_request_pool_apply(pool, _http_request_pool_detach);
}
/* }}} */

/* {{{ STATUS http_request_pool_send(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_send(http_request_pool *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
#ifdef HTTP_HAVE_EVENT
	CURLMcode rc;
	
	do {
		rc = curl_multi_socket_all(pool->ch, &pool->unfinished);
	} while (CURLM_CALL_MULTI_PERFORM == rc);
	
	if (CURLM_OK != rc) {
		http_error(HE_WARNING, HTTP_E_SOCKET, curl_multi_strerror(rc));
		return FAILURE;
	}
	
	event_base_dispatch(HTTP_G->request.pool.event.base);
	
	return SUCCESS;
#else
	
#	if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attempt to send %d requests of pool %p\n", zend_llist_count(&pool->handles), pool);
#	endif
	
	while (http_request_pool_perform(pool)) {
		if (SUCCESS != http_request_pool_select(pool)) {
#	ifdef PHP_WIN32
			/* see http://msdn.microsoft.com/library/en-us/winsock/winsock/windows_sockets_error_codes_2.asp */
			http_error_ex(HE_WARNING, HTTP_E_SOCKET, "WinSock error: %d", WSAGetLastError());
#	else
			http_error(HE_WARNING, HTTP_E_SOCKET, strerror(errno));
#	endif
			return FAILURE;
		}
	}
	
#	if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Finished sending %d HttpRequests of pool %p (still unfinished: %d)\n", zend_llist_count(&pool->handles), pool, pool->unfinished);
#	endif
	
	return SUCCESS;
#endif
}
/* }}} */

/* {{{ void http_request_pool_dtor(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destructing request pool %p\n", pool);
#endif
	
	pool->unfinished = 0;
	zend_llist_clean(&pool->finished);
	zend_llist_clean(&pool->handles);
	http_persistent_handle_release("http_request_pool", &pool->ch);
}
/* }}} */

#ifdef PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

/* {{{ STATUS http_request_pool_select(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_select(http_request_pool *pool)
{
#ifdef HTTP_HAVE_EVENT
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	http_error(HE_WARNING, HTTP_E_RUNTIME, "not implemented");
	return FAILURE;
#else
	int MAX;
	fd_set R, W, E;
	struct timeval timeout = {1, 0};
#	ifdef HAVE_CURL_MULTI_TIMEOUT
	long max_tout = 1000;
	
	if ((CURLM_OK == curl_multi_timeout(pool->ch, &max_tout)) && (max_tout != -1)) {
		timeout.tv_sec = max_tout / 1000;
		timeout.tv_usec = (max_tout % 1000) * 1000;
	}
#	endif

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(pool->ch, &R, &W, &E, &MAX)) {
		if (MAX == -1) {
			http_sleep((double) timeout.tv_sec + (double) (timeout.tv_usec / HTTP_MCROSEC));
			return SUCCESS;
		} else if (SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
#endif
}
/* }}} */

/* {{{ int http_request_pool_perform(http_request_pool *) */
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
#ifdef HTTP_HAVE_EVENT
	http_error(HE_WARNING, HTTP_E_RUNTIME, "not implemented");
	return FAILURE;
#else
	CURLMsg *msg;
	int remaining = 0;
	
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(pool->ch, &pool->unfinished));
	
	while ((msg = curl_multi_info_read(pool->ch, &remaining))) {
		if (CURLMSG_DONE == msg->msg) {
				if (CURLE_OK != msg->data.result) {
					http_request *r = NULL;
					curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &r);
					http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(msg->data.result), r?r->_error:"", r?r->url:"");
				}
				http_request_pool_apply_with_arg(pool, _http_request_pool_responsehandler, msg->easy_handle);
		}
	}
	
	return pool->unfinished;
#endif
}
/* }}} */

/* {{{ void http_request_pool_responsehandler(http_request_pool *, zval *, void *) */
int _http_request_pool_responsehandler(http_request_pool *pool, zval *req, void *ch)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	getObjectEx(http_request_object, obj, req);
	
	if ((!ch) || obj->request->ch == (CURL *) ch) {
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Fetching data from HttpRequest(#%d) %p of pool %p\n", Z_OBJ_HANDLE_P(req), obj, obj->pool);
#endif
		
		ZVAL_ADDREF(req);
		zend_llist_add_element(&obj->pool->finished, &req);
		http_request_object_responsehandler(obj, req);
		return 1;
	}
	return 0;
}
/* }}} */

/*#*/

/* {{{ static int http_request_pool_compare_handles(void *, void *) */
static int http_request_pool_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}
/* }}} */

#ifdef HTTP_HAVE_EVENT
static void http_request_pool_event_callback(int socket, short action, void *event_data)
{
	CURLMcode rc;
	CURLMsg *msg;
	int remaining;
	http_request_pool_event *ev = event_data;
	http_request_pool *pool = ev->pool;
	TSRMLS_FETCH_FROM_CTX(ev->pool->tsrm_ls);
	
#if HTTP_DEBUG_REQPOOLS
	{
		static const char event_strings[][20] = {"TIMEOUT","READ","TIMEOUT|READ","WRITE","TIMEOUT|WRITE","READ|WRITE","TIMEOUT|READ|WRITE","SIGNAL"};
		fprintf(stderr, "Event on socket %d (%s) event %p of pool %p\n", socket, event_strings[action], ev, pool);
	}
#endif
	
	do {
#ifdef HAVE_CURL_MULTI_SOCKET_ACTION
		switch (action & (EV_READ|EV_WRITE)) {
			case EV_READ:
				rc = curl_multi_socket_action(pool->ch, socket, CURL_CSELECT_IN, &pool->unfinished);
				break;
			case EV_WRITE:
				rc = curl_multi_socket_action(pool->ch, socket, CURL_CSELECT_OUT, &pool->unfinished);
				break;
			case EV_READ|EV_WRITE:
				rc = curl_multi_socket_action(pool->chm socket, CURL_CSELECT_IN|CURL_CSELECT_OUT, &pool->unfinished);
				break;
			default:
				http_error(HE_WARNING, HTTP_E_SOCKET, "Unknown event %d", (int) action);
				return;
		}
#else
		rc = curl_multi_socket(pool->ch, socket, &pool->unfinished);
#endif
	} while (CURLM_CALL_MULTI_PERFORM == rc);
	
	/* don't use 'ev' below here, as it might 've been freed in the socket callback */
	
	if (CURLM_OK != rc) {
		http_error(HE_WARNING, HTTP_E_SOCKET, curl_multi_strerror(rc));
	}
	
	while ((msg = curl_multi_info_read(pool->ch, &remaining))) {
		if (CURLMSG_DONE == msg->msg) {
			if (CURLE_OK != msg->data.result) {
				http_request *r = NULL;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &r);
				http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(msg->data.result), r?r->_error:"", r?r->url:"");
			}
			http_request_pool_apply_with_arg(pool, _http_request_pool_responsehandler, msg->easy_handle);
		}
	}
}

static int http_request_pool_socket_callback(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	int events = EV_PERSIST;
	http_request_pool *pool = socket_data;
	http_request_pool_event *ev = assign_data;
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	
	if (!ev) {
		ev = ecalloc(1, sizeof(http_request_pool_event));
		ev->pool = pool;
		curl_multi_assign(pool->ch, sock, ev);
	} else {
		event_del(&ev->evnt);
	}
	
#if HTTP_DEBUG_REQPOOLS
	{
		static const char action_strings[][8] = {"NONE", "IN", "OUT", "INOUT", "REMOVE"};
		fprintf(stderr, "Callback on socket %d (%s) event %p of pool %p\n", (int) sock, action_strings[action], ev, pool);
	}
#endif
	
	switch (action) {
		case CURL_POLL_IN:
			events |= EV_READ;
			break;
		case CURL_POLL_OUT:
			events |= EV_WRITE;
			break;
		case CURL_POLL_INOUT:
			events |= EV_READ|EV_WRITE;
			break;
			
		case CURL_POLL_REMOVE:
			efree(ev);
		case CURL_POLL_NONE:
			return 0;
			
		default:
			http_error_ex(HE_WARNING, HTTP_E_SOCKET, "Unknown socket action %d", action);
			return -1;
	}
	
	event_set(&ev->evnt, sock, events, http_request_pool_event_callback, ev);
	event_base_set(HTTP_G->request.pool.event.base, &ev->evnt);
	event_add(&ev->evnt, NULL);
	
	return 0;
}
#endif /* HTTP_HAVE_EVENT */

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

