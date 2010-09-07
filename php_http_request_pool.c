
#include "php_http.h"


#ifndef PHP_HTTP_DEBUG_REQPOOLS
#	define PHP_HTTP_DEBUG_REQPOOLS 0
#endif

#ifdef PHP_HTTP_HAVE_EVENT
typedef struct php_http_request_pool_event {
	struct event evnt;
	php_http_request_pool_t *pool;
} php_http_request_pool_event_t;

static void php_http_request_pool_timeout_callback(int socket, short action, void *event_data);
static void php_http_request_pool_event_callback(int socket, short action, void *event_data);
static int php_http_request_pool_socket_callback(CURL *easy, curl_socket_t s, int action, void *, void *);
static void php_http_request_pool_timer_callback(CURLM *multi, long timeout_ms, void *timer_data);
#endif

static int php_http_request_pool_compare_handles(void *h1, void *h2);

PHP_HTTP_API php_http_request_pool_t *php_http_request_pool_init(php_http_request_pool_t *pool TSRMLS_DC)
{
	zend_bool free_pool;

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initializing request pool %p\n", pool);
#endif

	if ((free_pool = (!pool))) {
		pool = emalloc(sizeof(php_http_request_pool_t));
		pool->ch = NULL;
	}

	if (SUCCESS != php_http_persistent_handle_acquire(ZEND_STRL("http_request_pool"), &pool->ch TSRMLS_CC)) {
		if (free_pool) {
			efree(pool);
		}
		return NULL;
	}

	TSRMLS_SET_CTX(pool->ts);

#ifdef PHP_HTTP_HAVE_EVENT
	pool->timeout = ecalloc(1, sizeof(struct event));
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETDATA, pool);
	curl_multi_setopt(pool->ch, CURLMOPT_SOCKETFUNCTION, php_http_request_pool_socket_callback);
	curl_multi_setopt(pool->ch, CURLMOPT_TIMERDATA, pool);
	curl_multi_setopt(pool->ch, CURLMOPT_TIMERFUNCTION, php_http_request_pool_timer_callback);
#endif

	pool->unfinished = 0;
	zend_llist_init(&pool->finished, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&pool->handles, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initialized request pool %p\n", pool);
#endif

	return pool;
}

PHP_HTTP_API STATUS php_http_request_pool_attach(php_http_request_pool_t *pool, zval *request)
{
#ifdef ZTS
	TSRMLS_FETCH_FROM_CTX(pool->ts);
#endif
	php_http_request_object_t *req = zend_object_store_get_object(request TSRMLS_CC);

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attaching HttpRequest(#%d) %p to pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif

	if (req->pool) {
		php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is already member of %s HttpRequestPool", Z_OBJ_HANDLE_P(request), req->pool == pool ? "this" : "another");
	} else if (SUCCESS != php_http_request_object_requesthandler(req, request)) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not initialize HttpRequest object(#%d) for attaching to the HttpRequestPool", Z_OBJ_HANDLE_P(request));
	} else {
		CURLMcode code = curl_multi_add_handle(pool->ch, req->request->ch);

		if (CURLM_OK != code) {
			php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "Could not attach HttpRequest object(#%d) to the HttpRequestPool: %s", Z_OBJ_HANDLE_P(request), curl_multi_strerror(code));
		} else {
			req->pool = pool;

			Z_ADDREF_P(request);
			zend_llist_add_element(&pool->handles, &request);
			++pool->unfinished;

#if PHP_HTTP_DEBUG_REQPOOLS
			fprintf(stderr, "> %d HttpRequests attached to pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
			return SUCCESS;
		}
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_pool_detach(php_http_request_pool_t *pool, zval *request)
{
	CURLMcode code;
#ifdef ZTS
	TSRMLS_FETCH_FROM_CTX(pool->ts);
#endif
	php_http_request_object_t *req = zend_object_store_get_object(request TSRMLS_CC);

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching HttpRequest(#%d) %p from pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif

	if (!req->pool) {
		/* not attached to any pool */
#if PHP_HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "HttpRequest object(#%d) %p is not attached to any HttpRequestPool\n", Z_OBJ_HANDLE_P(request), req);
#endif
	} else if (req->pool != pool) {
		php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is not attached to this HttpRequestPool", Z_OBJ_HANDLE_P(request));
	} else if (req->request->_progress.in_cb) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "HttpRequest object(#%d) cannot be detached from the HttpRequestPool while executing the progress callback", Z_OBJ_HANDLE_P(request));
	} else if (CURLM_OK != (code = curl_multi_remove_handle(pool->ch, req->request->ch))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_POOL, "Could not detach HttpRequest object(#%d) from the HttpRequestPool: %s", Z_OBJ_HANDLE_P(request), curl_multi_strerror(code));
	} else {
		req->pool = NULL;
		zend_llist_del_element(&pool->finished, request, php_http_request_pool_compare_handles);
		zend_llist_del_element(&pool->handles, request, php_http_request_pool_compare_handles);

#if PHP_HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "> %d HttpRequests remaining in pool %p\n", zend_llist_count(&pool->handles), pool);
#endif

		return SUCCESS;
	}
	return FAILURE;
}

PHP_HTTP_API void php_http_request_pool_apply(php_http_request_pool_t *pool, php_http_request_pool_apply_func_t cb)
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

PHP_HTTP_API void php_http_request_pool_apply_with_arg(php_http_request_pool_t *pool, php_http_request_pool_apply_with_arg_func_t cb, void *arg)
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

PHP_HTTP_API void php_http_request_pool_detach_all(php_http_request_pool_t *pool)
{
#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching %d requests from pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	php_http_request_pool_apply(pool, php_http_request_pool_detach);
}

PHP_HTTP_API STATUS php_http_request_pool_send(php_http_request_pool_t *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->ts);

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attempt to send %d requests of pool %p\n", zend_llist_count(&pool->handles), pool);
#endif

#ifdef PHP_HTTP_HAVE_EVENT
	if (pool->useevents) {
		do {
#if PHP_HTTP_DEBUG_REQPOOLS
			fprintf(stderr, "& Starting event dispatcher of pool %p\n", pool);
#endif
			event_base_dispatch(PHP_HTTP_G->request_pool.event_base);
		} while (pool->unfinished);
	} else
#endif
	{
		while (php_http_request_pool_perform(pool)) {
			if (SUCCESS != php_http_request_pool_select(pool, NULL)) {
#ifdef PHP_WIN32
				/* see http://msdn.microsoft.com/library/en-us/winsock/winsock/windows_sockets_error_codes_2.asp */
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "WinSock error: %d", WSAGetLastError());
#else
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, strerror(errno));
#endif
				return FAILURE;
			}
		}
	}

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Finished sending %d HttpRequests of pool %p (still unfinished: %d)\n", zend_llist_count(&pool->handles), pool, pool->unfinished);
#endif

	return SUCCESS;
}

PHP_HTTP_API void php_http_request_pool_dtor(php_http_request_pool_t *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->ts);

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destructing request pool %p\n", pool);
#endif

#ifdef PHP_HTTP_HAVE_EVENT
	efree(pool->timeout);
#endif

	php_http_request_pool_detach_all(pool);

	pool->unfinished = 0;
	zend_llist_clean(&pool->finished);
	zend_llist_clean(&pool->handles);
	php_http_persistent_handle_release(ZEND_STRL("php_http_request_pool_t"), &pool->ch TSRMLS_CC);
}

PHP_HTTP_API void php_http_request_pool_free(php_http_request_pool_t **pool) {
	if (*pool) {
		php_http_request_pool_dtor(*pool);
		efree(*pool);
		*pool = NULL;
	}
}

#ifdef PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

PHP_HTTP_API STATUS php_http_request_pool_select(php_http_request_pool_t *pool, struct timeval *custom_timeout)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout;

#ifdef PHP_HTTP_HAVE_EVENT
	if (pool->useevents) {
		TSRMLS_FETCH_FROM_CTX(pool->ts);
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented; use HttpRequest callbacks");
		return FAILURE;
	}
#endif

	if (custom_timeout && timerisset(custom_timeout)) {
		timeout = *custom_timeout;
	} else {
		php_http_request_pool_timeout(pool, &timeout);
	}

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(pool->ch, &R, &W, &E, &MAX)) {
		if (MAX == -1) {
			php_http_sleep((double) timeout.tv_sec + (double) (timeout.tv_usec / PHP_HTTP_MCROSEC));
			return SUCCESS;
		} else if (SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

PHP_HTTP_API int php_http_request_pool_perform(php_http_request_pool_t *pool)
{
	TSRMLS_FETCH_FROM_CTX(pool->ts);

#ifdef PHP_HTTP_HAVE_EVENT
	if (pool->useevents) {
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented; use HttpRequest callbacks");
		return FAILURE;
	}
#endif

	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(pool->ch, &pool->unfinished));

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "%u unfinished requests of pool %p remaining\n", pool->unfinished, pool);
#endif

	php_http_request_pool_responsehandler(pool);

	return pool->unfinished;
}

void php_http_request_pool_responsehandler(php_http_request_pool_t *pool)
{
	CURLMsg *msg;
	int remaining = 0;
	TSRMLS_FETCH_FROM_CTX(pool->ts);

	do {
		msg = curl_multi_info_read(pool->ch, &remaining);
		if (msg && CURLMSG_DONE == msg->msg) {
			if (CURLE_OK != msg->data.result) {
				php_http_request_storage_t *st = php_http_request_storage_get(msg->easy_handle);
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(msg->data.result), STR_PTR(st->errorbuffer), STR_PTR(st->url));
			}
			php_http_request_pool_apply_with_arg(pool, php_http_request_pool_apply_responsehandler, msg->easy_handle);
		}
	} while (remaining);
}

int php_http_request_pool_apply_responsehandler(php_http_request_pool_t *pool, zval *req, void *ch)
{
#ifdef ZTS
	TSRMLS_FETCH_FROM_CTX(pool->ts);
#endif
	php_http_request_object_t *obj = zend_object_store_get_object(req TSRMLS_CC);

	if ((!ch) || obj->request->ch == (CURL *) ch) {

#if PHP_HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Fetching data from HttpRequest(#%d) %p of pool %p\n", Z_OBJ_HANDLE_P(req), obj, obj->pool);
#endif

		Z_ADDREF_P(req);
		zend_llist_add_element(&obj->pool->finished, &req);
		php_http_request_object_responsehandler(obj, req);
		return 1;
	}
	return 0;
}

struct timeval *php_http_request_pool_timeout(php_http_request_pool_t *pool, struct timeval *timeout)
{
#ifdef HAVE_CURL_MULTI_TIMEOUT
	long max_tout = 1000;

	if ((CURLM_OK == curl_multi_timeout(pool->ch, &max_tout)) && (max_tout > 0)) {
		timeout->tv_sec = max_tout / 1000;
		timeout->tv_usec = (max_tout % 1000) * 1000;
	} else {
#endif
		timeout->tv_sec = 0;
		timeout->tv_usec = 1000;
#ifdef HAVE_CURL_MULTI_TIMEOUT
	}
#endif

#if PHP_HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Calculating timeout (%lu, %lu) of pool %p\n", (ulong) timeout->tv_sec, (ulong) timeout->tv_usec, pool);
#endif

	return timeout;
}

/*#*/

static int php_http_request_pool_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}

#ifdef PHP_HTTP_HAVE_EVENT

static void php_http_request_pool_timeout_callback(int socket, short action, void *event_data)
{
	php_http_request_pool_t *pool = event_data;

	if (pool->useevents) {
		CURLMcode rc;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

#if PHP_HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Timeout occurred of pool %p\n", pool);
#endif

		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket(pool->ch, CURL_SOCKET_TIMEOUT, &pool->unfinished)));

		if (CURLM_OK != rc) {
			php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, curl_multi_strerror(rc));
		}

		php_http_request_pool_responsehandler(pool);
	}
}

static void php_http_request_pool_event_callback(int socket, short action, void *event_data)
{
	php_http_request_pool_event_t *ev = event_data;
	php_http_request_pool_t *pool = ev->pool;

	if (pool->useevents) {
		CURLMcode rc = CURLE_OK;
		TSRMLS_FETCH_FROM_CTX(ev->pool->ts);

#if PHP_HTTP_DEBUG_REQPOOLS
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
					php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "Unknown event %d", (int) action);
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
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, curl_multi_strerror(rc));
				break;
		}

		php_http_request_pool_responsehandler(pool);

		/* remove timeout if there are no transfers left */
		if (!pool->unfinished && event_initialized(pool->timeout) && event_pending(pool->timeout, EV_TIMEOUT, NULL)) {
			event_del(pool->timeout);
#if PHP_HTTP_DEBUG_REQPOOLS
			fprintf(stderr, "Removed timeout of pool %p\n", pool);
#endif
		}
	}
}

static int php_http_request_pool_socket_callback(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_request_pool_t *pool = socket_data;

	if (pool->useevents) {
		int events = EV_PERSIST;
		php_http_request_pool_event_t *ev = assign_data;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		if (!ev) {
			ev = ecalloc(1, sizeof(php_http_request_pool_event_t));
			ev->pool = pool;
			curl_multi_assign(pool->ch, sock, ev);
			event_base_set(PHP_HTTP_G->request_pool.event_base, &ev->evnt);
		} else {
			event_del(&ev->evnt);
		}

#if PHP_HTTP_DEBUG_REQPOOLS
		{
			static const char action_strings[][8] = {"NONE", "IN", "OUT", "INOUT", "REMOVE"};
			php_http_request_t *r;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &r);
			fprintf(stderr, "Callback on socket %2d (%8s) event %p of pool %p (%d)\n", (int) sock, action_strings[action], ev, pool, pool->unfinished);
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
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "Unknown socket action %d", action);
				return -1;
		}

		event_set(&ev->evnt, sock, events, php_http_request_pool_event_callback, ev);
		event_add(&ev->evnt, NULL);
	}

	return 0;
}

static void php_http_request_pool_timer_callback(CURLM *multi, long timeout_ms, void *timer_data)
{
	php_http_request_pool_t *pool = timer_data;

	if (pool->useevents) {
		TSRMLS_FETCH_FROM_CTX(pool->ts);
		struct timeval timeout;

		if (!event_initialized(pool->timeout)) {
			event_set(pool->timeout, -1, 0, php_http_request_pool_timeout_callback, pool);
			event_base_set(PHP_HTTP_G->request_pool.event_base, pool->timeout);
		} else if (event_pending(pool->timeout, EV_TIMEOUT, NULL)) {
			event_del(pool->timeout);
		}

		if (timeout_ms > 0) {
			timeout.tv_sec = timeout_ms / 1000;
			timeout.tv_usec = (timeout_ms % 1000) * 1000;
		} else {
			php_http_request_pool_timeout(pool, &timeout);
		}

		event_add(pool->timeout, &timeout);

#if PHP_HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Updating timeout %lu (%lu, %lu) of pool %p\n", (ulong) timeout_ms, (ulong) timeout.tv_sec, (ulong) timeout.tv_usec, pool);
#endif
	}
}
#endif /* HAVE_EVENT */

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequestPool, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequestPool, method, 0)
#define PHP_HTTP_REQPOOL_ME(method, visibility)	PHP_ME(HttpRequestPool, method, PHP_HTTP_ARGS(HttpRequestPool, method), visibility)

PHP_HTTP_EMPTY_ARGS(__construct);

PHP_HTTP_EMPTY_ARGS(__destruct);
PHP_HTTP_EMPTY_ARGS(reset);

PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(send);
PHP_HTTP_EMPTY_ARGS(socketPerform);
PHP_HTTP_BEGIN_ARGS(socketSelect, 0)
	PHP_HTTP_ARG_VAL(timeout, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(valid);
PHP_HTTP_EMPTY_ARGS(current);
PHP_HTTP_EMPTY_ARGS(key);
PHP_HTTP_EMPTY_ARGS(next);
PHP_HTTP_EMPTY_ARGS(rewind);

PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_EMPTY_ARGS(getAttachedRequests);
PHP_HTTP_EMPTY_ARGS(getFinishedRequests);

PHP_HTTP_BEGIN_ARGS(enablePipelining, 0)
	PHP_HTTP_ARG_VAL(enable, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(enableEvents, 0)
	PHP_HTTP_ARG_VAL(enable, 0)
PHP_HTTP_END_ARGS;

zend_class_entry *php_http_request_pool_class_entry;
zend_function_entry php_http_request_pool_method_entry[] = {
	PHP_HTTP_REQPOOL_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_REQPOOL_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_HTTP_REQPOOL_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(send, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(reset, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(socketPerform, ZEND_ACC_PROTECTED)
	PHP_HTTP_REQPOOL_ME(socketSelect, ZEND_ACC_PROTECTED)

	/* implements Iterator */
	PHP_HTTP_REQPOOL_ME(valid, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(current, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(key, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(next, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(rewind, ZEND_ACC_PUBLIC)

	/* implmenents Countable */
	PHP_HTTP_REQPOOL_ME(count, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(getAttachedRequests, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(getFinishedRequests, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQPOOL_ME(enablePipelining, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQPOOL_ME(enableEvents, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_request_pool_object_handlers;

zend_object_value php_http_request_pool_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	php_http_request_pool_object_t *o;

	o = ecalloc(1, sizeof(php_http_request_pool_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	php_http_request_pool_init(&o->pool TSRMLS_CC);

	ov.handle = zend_objects_store_put(o, NULL, php_http_request_pool_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_request_pool_object_handlers;

	return ov;
}

void php_http_request_pool_object_free(void *object TSRMLS_DC)
{
	php_http_request_pool_object_t *o = (php_http_request_pool_object_t *) object;

	php_http_request_pool_dtor(&o->pool);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static void php_http_request_pool_object_llist2array(zval **req, zval *array TSRMLS_DC)
{
	Z_ADDREF_P(*req);
	add_next_index_zval(array, *req);
}

/* ### USERLAND ### */

PHP_METHOD(HttpRequestPool, __construct)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		int argc;
		zval ***argv;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "*", &argv, &argc)) {
			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(request_pool)) {
				int i;
				php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				for (i = 0; i < argc; ++i) {
					if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), php_http_request_class_entry TSRMLS_CC)) {
						php_http_request_pool_attach(&obj->pool, *(argv[i]));
					}
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestPool, __destruct)
{
	php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	if (SUCCESS != zend_parse_parameters_none()) {
		; /* we always want to clean up */
	}

	php_http_request_pool_detach_all(&obj->pool);
}

PHP_METHOD(HttpRequestPool, reset)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->iterator.pos = 0;
		php_http_request_pool_detach_all(&obj->pool);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, attach)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		zval *request;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_request_class_entry)) {
			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
				php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				if (obj->iterator.pos > 0 && obj->iterator.pos < zend_llist_count(&obj->pool.handles)) {
					php_http_error(HE_THROW, PHP_HTTP_E_REQUEST_POOL, "Cannot attach to the HttpRequestPool while the iterator is active");
				} else {
					RETVAL_SUCCESS(php_http_request_pool_attach(&obj->pool, request));
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestPool, detach)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		zval *request;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, php_http_request_class_entry)) {
			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(request_pool)) {
				php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				obj->iterator.pos = -1;
				RETVAL_SUCCESS(php_http_request_pool_detach(&obj->pool, request));
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestPool, send)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			with_error_handling(EH_THROW, PHP_HTTP_EX_CE(request_pool)) {
				php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				RETVAL_SUCCESS(php_http_request_pool_send(&obj->pool));
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestPool, socketPerform)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (0 < php_http_request_pool_perform(&obj->pool)) {
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, socketSelect)
{
	double timeout = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|d", &timeout)) {
		struct timeval timeout_val;
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		timeout_val.tv_sec = (time_t) timeout;
		timeout_val.tv_usec = PHP_HTTP_USEC(timeout) % PHP_HTTP_MCROSEC;

		RETURN_SUCCESS(php_http_request_pool_select(&obj->pool, timeout ? &timeout_val : NULL));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, valid)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_BOOL(obj->iterator.pos >= 0 && obj->iterator.pos < zend_llist_count(&obj->pool.handles));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, current)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator.pos < zend_llist_count(&obj->pool.handles)) {
			long pos = 0;
			zval **current = NULL;
			zend_llist_position lpos;

			for (	current = zend_llist_get_first_ex(&obj->pool.handles, &lpos);
					current && obj->iterator.pos != pos++;
					current = zend_llist_get_next_ex(&obj->pool.handles, &lpos));
			if (current) {
				RETURN_OBJECT(*current, 1);
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, key)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(obj->iterator.pos);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, next)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		++obj->iterator.pos;
	}
}

PHP_METHOD(HttpRequestPool, rewind)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->iterator.pos = 0;
	}
}

PHP_METHOD(HttpRequestPool, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG((long) zend_llist_count(&obj->pool.handles));
	}
}

PHP_METHOD(HttpRequestPool, getAttachedRequests)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		array_init(return_value);
		zend_llist_apply_with_argument(&obj->pool.handles,
			(llist_apply_with_arg_func_t) php_http_request_pool_object_llist2array,
			return_value TSRMLS_CC);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, getFinishedRequests)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		array_init(return_value);
		zend_llist_apply_with_argument(&obj->pool.finished,
				(llist_apply_with_arg_func_t) php_http_request_pool_object_llist2array,
				return_value TSRMLS_CC);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, enablePipelining)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (CURLM_OK == curl_multi_setopt(obj->pool.ch, CURLMOPT_PIPELINING, (long) enable)) {
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestPool, enableEvents)
{
	zend_bool enable = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &enable)) {
#if PHP_HTTP_HAVE_EVENT
		php_http_request_pool_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		obj->pool.useevents = enable;
		RETURN_TRUE;
#endif
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_request_pool)
{
	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request_pool"), curl_multi_init, (php_http_persistent_handle_dtor_t) curl_multi_cleanup, NULL TSRMLS_CC)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http\\request, Pool, http_request_pool, php_http_object_class_entry, 0);
	php_http_request_pool_class_entry->create_object = php_http_request_pool_object_new;
	memcpy(&php_http_request_pool_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_request_pool_object_handlers.clone_obj = NULL;

	zend_class_implements(php_http_request_pool_class_entry TSRMLS_CC, 2, spl_ce_Countable, zend_ce_iterator);

	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_request_pool)
{
#ifdef PHP_HTTP_HAVE_EVENT
	if (!PHP_HTTP_G->request_pool.event_base && !(PHP_HTTP_G->request_pool.event_base = event_init())) {
		return FAILURE;
	}
#endif

	return SUCCESS;
}


