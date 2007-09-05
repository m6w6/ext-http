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

static void http_request_pool_timeout_callback(int socket, short action, void *event_data);
static void http_request_pool_event_callback(int socket, short action, void *event_data);
static int http_request_pool_socket_callback(CURL *easy, curl_socket_t s, int action, void *, void *);
static void http_request_pool_timer_callback(CURLM *multi, long timeout_ms, void *timer_data);
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
	
#ifdef HTTP_HAVE_EVENT
	pool->timeout = ecalloc(1, sizeof(struct event));
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETDATA, pool);
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETFUNCTION, http_request_pool_socket_callback);
	curl_multi_setopt(pool->ch, CURLMOPT_TIMERDATA, pool);
	curl_multi_setopt(pool->ch, CURLMOPT_TIMERFUNCTION, http_request_pool_timer_callback);
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
		
		if (CURLM_OK != code) {
			http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "Could not attach HttpRequest object(#%d) to the HttpRequestPool: %s", Z_OBJ_HANDLE_P(request), curl_multi_strerror(code));
		} else {
			req->pool = pool;

			ZVAL_ADDREF(request);
			zend_llist_add_element(&pool->handles, &request);
			++pool->unfinished;

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
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attempt to send %d requests of pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	
#ifdef HTTP_HAVE_EVENT
	if (pool->useevents) {
		do {
			while (CURLM_CALL_MULTI_PERFORM == curl_multi_socket_all(pool->ch, &pool->unfinished));
			event_base_dispatch(HTTP_G->request.pool.event.base);
		} while (pool->unfinished);
	} else
#endif
	{
		while (http_request_pool_perform(pool)) {
			if (SUCCESS != http_request_pool_select(pool)) {
#ifdef PHP_WIN32
				/* see http://msdn.microsoft.com/library/en-us/winsock/winsock/windows_sockets_error_codes_2.asp */
				http_error_ex(HE_WARNING, HTTP_E_SOCKET, "WinSock error: %d", WSAGetLastError());
#else
				http_error(HE_WARNING, HTTP_E_SOCKET, strerror(errno));
#endif
				return FAILURE;
			}
		}
	}
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Finished sending %d HttpRequests of pool %p (still unfinished: %d)\n", zend_llist_count(&pool->handles), pool, pool->unfinished);
#endif
	
	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_pool_dtor(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destructing request pool %p\n", pool);
#endif
	
#ifdef HTTP_HAVE_EVENT
	efree(pool->timeout);
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
	int MAX;
	fd_set R, W, E;
	struct timeval timeout;

#ifdef HTTP_HAVE_EVENT
	if (pool->useevents) {
		TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
		http_error(HE_WARNING, HTTP_E_RUNTIME, "not implemented; use HttpRequest callbacks");
		return FAILURE;
	}
#endif
	
	http_request_pool_timeout(pool, &timeout);
	
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
}
/* }}} */

/* {{{ int http_request_pool_perform(http_request_pool *) */
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	
#ifdef HTTP_HAVE_EVENT
	if (pool->useevents) {
		http_error(HE_WARNING, HTTP_E_RUNTIME, "not implemented; use HttpRequest callbacks");
		return FAILURE;
	}
#endif
	
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(pool->ch, &pool->unfinished));
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "%u unfinished requests of pool %p remaining\n", pool->unfinished, pool);
#endif
	
	http_request_pool_responsehandler(pool);
	
	return pool->unfinished;
}
/* }}} */

/* {{{ void http_request_pool_responsehandler(http_request_pool *) */
void _http_request_pool_responsehandler(http_request_pool *pool)
{
	CURLMsg *msg;
	int remaining = 0;
	TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
	
	do {
		msg = curl_multi_info_read(pool->ch, &remaining);
		if (msg && CURLMSG_DONE == msg->msg) {
			if (CURLE_OK != msg->data.result) {
				http_request *r = NULL;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &r);
				http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(msg->data.result), r?r->_error:"", r?r->url:"");
			}
			http_request_pool_apply_with_arg(pool, _http_request_pool_apply_responsehandler, msg->easy_handle);
		}
	} while (remaining);
}
/* }}} */

/* {{{ int http_request_pool_apply_responsehandler(http_request_pool *, zval *, void *) */
int _http_request_pool_apply_responsehandler(http_request_pool *pool, zval *req, void *ch)
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

/* {{{ struct timeval *_http_request_pool_timeout(http_request_pool *, struct timeval *) */
struct timeval *_http_request_pool_timeout(http_request_pool *pool, struct timeval *timeout)
{
#ifdef HAVE_CURL_MULTI_TIMEOUT
	long max_tout = 1000;
	
	if ((CURLM_OK == curl_multi_timeout(pool->ch, &max_tout)) && (max_tout != -1)) {
		timeout->tv_sec = max_tout / 1000;
		timeout->tv_usec = (max_tout % 1000) * 1000;
	} else {
#endif
		timeout->tv_sec = 1;
		timeout->tv_usec = 0;
#ifdef HAVE_CURL_MULTI_TIMEOUT
	}
#endif
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Calculating timeout (%lu, %lu) of pool %p\n", (ulong) timeout->tv_sec, (ulong) timeout->tv_usec, pool);
#endif
	
	return timeout;
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
/* {{{ static void http_request_pool_timeout_callback(int, short, void *) */
static void http_request_pool_timeout_callback(int socket, short action, void *event_data)
{
	http_request_pool *pool = event_data;
	
	if (pool->useevents) {
		CURLMcode rc;
		TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Timeout occurred of pool %p\n", pool);
#endif
		
		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket(pool->ch, CURL_SOCKET_TIMEOUT, &pool->unfinished)));
		
		if (CURLM_OK != rc) {
			http_error(HE_WARNING, HTTP_E_SOCKET, curl_multi_strerror(rc));
		}
#if 0
		http_request_pool_timer_callback(pool->ch, 1000, pool);
#endif
	}
}
/* }}} */

/* {{{ static void http_request_pool_event_callback(int, short, void *) */
static void http_request_pool_event_callback(int socket, short action, void *event_data)
{
	http_request_pool_event *ev = event_data;
	http_request_pool *pool = ev->pool;
	
	if (pool->useevents) {
		CURLMcode rc = CURLE_OK;
		TSRMLS_FETCH_FROM_CTX(ev->pool->tsrm_ls);
			
#if HTTP_DEBUG_REQPOOLS
		{
			static const char event_strings[][20] = {"NONE","TIMEOUT","READ","TIMEOUT|READ","WRITE","TIMEOUT|WRITE","READ|WRITE","TIMEOUT|READ|WRITE","SIGNAL"};
			fprintf(stderr, "Event on socket %d (%s) event %p of pool %p\n", socket, event_strings[action], ev, pool);
		}
#endif
		
		/* don't use 'ev' below this loop as it might 've been freed in the socket callback */
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
					rc = curl_multi_socket_action(pool->ch, socket, CURL_CSELECT_IN|CURL_CSELECT_OUT, &pool->unfinished);
					break;
				default:
					http_error_ex(HE_WARNING, HTTP_E_SOCKET, "Unknown event %d", (int) action);
					return;
			}
#else
			rc = curl_multi_socket(pool->ch, socket, &pool->unfinished);
#endif
		} while (CURLM_CALL_MULTI_PERFORM == rc);
		
		switch (rc) {
			case CURLM_BAD_SOCKET:
#if 0
				fprintf(stderr, "!!! Bad socket: %d (%d)\n", socket, (int) action);
#endif
			case CURLM_OK:
				break;
			default:
				http_error(HE_WARNING, HTTP_E_SOCKET, curl_multi_strerror(rc));
				break;
		}
		
		http_request_pool_responsehandler(pool);
		
		/* remove timeout if there are no transfers left */
		if (!pool->unfinished && event_initialized(pool->timeout) && event_pending(pool->timeout, EV_TIMEOUT, NULL)) {
			event_del(pool->timeout);
#if HTTP_DEBUG_REQPOOLS
			fprintf(stderr, "Removed timeout of pool %p\n", pool);
#endif
		}
	}
}
/* }}} */

/* {{{ static int http_request_pool_socket_callback(CURL *, curl_socket_t, int, void *, void *) */
static int http_request_pool_socket_callback(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	http_request_pool *pool = socket_data;
	
	if (pool->useevents) {
		int events = EV_PERSIST;
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
			http_request *r;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &r);
			fprintf(stderr, "Callback on socket %d (%s) event %p of pool %p (%s)\n", (int) sock, action_strings[action], ev, pool, r->url);
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
	}
	
	return 0;
}
/* }}} */

/* {{{ static void http_request_pool_timer_callback(CURLM *, long, void*) */
static void http_request_pool_timer_callback(CURLM *multi, long timeout_ms, void *timer_data)
{
	http_request_pool *pool = timer_data;
	
	if (pool->useevents) {
		TSRMLS_FETCH_FROM_CTX(pool->tsrm_ls);
		struct timeval timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
		
		if (event_initialized(pool->timeout) && event_pending(pool->timeout, EV_TIMEOUT, NULL)) {
			event_del(pool->timeout);
		}
		
		event_set(pool->timeout, -1, 0, http_request_pool_timeout_callback, pool);
		event_base_set(HTTP_G->request.pool.event.base, pool->timeout);
		event_add(pool->timeout, &timeout);
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Updating timeout (%lu, %lu) of pool %p\n", (ulong) timeout.tv_sec, (ulong) timeout.tv_usec, pool);
#endif
	}
}
/* }}} */
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

