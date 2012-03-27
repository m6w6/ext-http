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

#if PHP_HTTP_HAVE_CURL

#if PHP_HTTP_HAVE_EVENT
#	include <event.h>
#endif

typedef struct php_http_client_pool_curl {
	CURLM *handle;

	int unfinished;  /* int because of curl_multi_perform() */

#if PHP_HTTP_HAVE_EVENT
	struct event *timeout;
	unsigned useevents:1;
	unsigned runsocket:1;
#endif
} php_http_client_pool_curl_t;

static void *php_http_curlm_ctor(void *opaque TSRMLS_DC)
{
	return curl_multi_init();
}

static void php_http_curlm_dtor(void *opaque, void *handle TSRMLS_DC)
{
	curl_multi_cleanup(handle);
}


static void php_http_client_pool_curl_responsehandler(php_http_client_pool_t *pool)
{
	int remaining = 0;
	zval **requests;
	php_http_client_pool_curl_t *curl = pool->ctx;
	TSRMLS_FETCH_FROM_CTX(pool->ts);

	do {
		CURLMsg *msg = curl_multi_info_read(curl->handle, &remaining);

		if (msg && CURLMSG_DONE == msg->msg) {
			zval **request;

			if (CURLE_OK != msg->data.result) {
				php_http_client_curl_storage_t *st = get_storage(msg->easy_handle);
				php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT, "%s; %s (%s)", curl_easy_strerror(msg->data.result), STR_PTR(st->errorbuffer), STR_PTR(st->url));
			}

			php_http_client_pool_requests(pool, &requests, NULL);
			for (request = requests; *request; ++request) {
				php_http_client_object_t *obj = zend_object_store_get_object(*request TSRMLS_CC);

				if (msg->easy_handle == ((php_http_client_curl_t *) (obj->client->ctx))->handle) {
					Z_ADDREF_PP(request);
					zend_llist_add_element(&pool->clients.finished, request);
					php_http_client_object_handle_response(*request TSRMLS_CC);
				}

				zval_ptr_dtor(request);
			}
			efree(requests);
		}
	} while (remaining);
}


#if PHP_HTTP_HAVE_EVENT

typedef struct php_http_client_pool_event {
	struct event evnt;
	php_http_client_pool_t *pool;
} php_http_client_pool_event_t;

static inline int etoca(short action) {
	switch (action & (EV_READ|EV_WRITE)) {
		case EV_READ:
			return CURL_CSELECT_IN;
			break;
		case EV_WRITE:
			return CURL_CSELECT_OUT;
			break;
		case EV_READ|EV_WRITE:
			return CURL_CSELECT_IN|CURL_CSELECT_OUT;
			break;
		default:
			return 0;
	}
}

static void php_http_client_pool_curl_timeout_callback(int socket, short action, void *event_data)
{
	php_http_client_pool_t *pool = event_data;
	php_http_client_pool_curl_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "T");
#endif
	if (curl->useevents) {
		CURLMcode rc;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket_action(curl->handle, socket, etoca(action), &curl->unfinished)));

		if (CURLM_OK != rc) {
			php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "%s",  curl_multi_strerror(rc));
		}

		php_http_client_pool_curl_responsehandler(pool);
	}
}

static void php_http_client_pool_curl_event_callback(int socket, short action, void *event_data)
{
	php_http_client_pool_t *pool = event_data;
	php_http_client_pool_curl_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif
	if (curl->useevents) {
		CURLMcode rc = CURLE_OK;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		while (CURLM_CALL_MULTI_PERFORM == (rc = curl_multi_socket_action(curl->handle, socket, etoca(action), &curl->unfinished)));

		if (CURLM_OK != rc) {
			php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "%s", curl_multi_strerror(rc));
		}

		php_http_client_pool_curl_responsehandler(pool);

		/* remove timeout if there are no transfers left */
		if (!curl->unfinished && event_initialized(curl->timeout) && event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
			event_del(curl->timeout);
		}
	}
}

static int php_http_client_pool_curl_socket_callback(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_client_pool_t *pool = socket_data;
	php_http_client_pool_curl_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "S");
#endif
	if (curl->useevents) {
		int events = EV_PERSIST;
		php_http_client_pool_event_t *ev = assign_data;
		TSRMLS_FETCH_FROM_CTX(pool->ts);

		if (!ev) {
			ev = ecalloc(1, sizeof(php_http_client_pool_event_t));
			ev->pool = pool;
			curl_multi_assign(curl->handle, sock, ev);
			event_base_set(PHP_HTTP_G->curl.event_base, &ev->evnt);
		} else {
			event_del(&ev->evnt);
		}

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
				/* no break */
			case CURL_POLL_NONE:
				return 0;

			default:
				php_http_error(HE_WARNING, PHP_HTTP_E_SOCKET, "Unknown socket action %d", action);
				return -1;
		}

		event_set(&ev->evnt, sock, events, php_http_client_pool_curl_event_callback, pool);
		event_add(&ev->evnt, NULL);
	}

	return 0;
}

static void php_http_client_pool_curl_timer_callback(CURLM *multi, long timeout_ms, void *timer_data)
{
	php_http_client_pool_t *pool = timer_data;
	php_http_client_pool_curl_t *curl = pool->ctx;

#if DBG_EVENTS
	fprintf(stderr, "%ld", timeout_ms);
#endif
	if (curl->useevents) {

		if (timeout_ms < 0) {
			php_http_client_pool_curl_timeout_callback(CURL_SOCKET_TIMEOUT, CURL_CSELECT_IN|CURL_CSELECT_OUT, pool);
		} else if (timeout_ms > 0 || !event_initialized(curl->timeout) || !event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
			struct timeval timeout;
			TSRMLS_FETCH_FROM_CTX(pool->ts);

			if (!event_initialized(curl->timeout)) {
				event_set(curl->timeout, -1, 0, php_http_client_pool_curl_timeout_callback, pool);
				event_base_set(PHP_HTTP_G->curl.event_base, curl->timeout);
			} else if (event_pending(curl->timeout, EV_TIMEOUT, NULL)) {
				event_del(curl->timeout);
			}

			timeout.tv_sec = timeout_ms / 1000;
			timeout.tv_usec = (timeout_ms % 1000) * 1000;

			event_add(curl->timeout, &timeout);
		}
	}
}

#endif /* HAVE_EVENT */


/* pool handler ops */

static php_http_client_pool_t *php_http_client_pool_curl_init(php_http_client_pool_t *h, void *handle)
{
	php_http_client_pool_curl_t *curl;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (!handle && !(handle = php_http_resource_factory_handle_ctor(h->rf TSRMLS_CC))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_POOL, "could not initialize curl pool handle");
		return NULL;
	}

	curl = ecalloc(1, sizeof(*curl));
	curl->handle = handle;
	curl->unfinished = 0;
	h->ctx = curl;

	return h;
}

static void php_http_client_pool_curl_dtor(php_http_client_pool_t *h)
{
	php_http_client_pool_curl_t *curl = h->ctx;
	TSRMLS_FETCH_FROM_CTX(h->ts);

#if PHP_HTTP_HAVE_EVENT
	if (curl->timeout) {
		efree(curl->timeout);
		curl->timeout = NULL;
	}
#endif
	curl->unfinished = 0;
	php_http_client_pool_reset(h);

	php_http_resource_factory_handle_dtor(h->rf, curl->handle TSRMLS_CC);

	efree(curl);
	h->ctx = NULL;
}

static STATUS php_http_client_pool_curl_attach(php_http_client_pool_t *h, php_http_client_t *r, php_http_message_t *m)
{
	php_http_client_pool_curl_t *curl = h->ctx;
	php_http_client_curl_t *recurl = r->ctx;
	CURLMcode rs;
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (SUCCESS != php_http_client_curl_prepare(r, m)) {
		return FAILURE;
	}

	if (CURLM_OK == (rs = curl_multi_add_handle(curl->handle, recurl->handle))) {
		++curl->unfinished;
		return SUCCESS;
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_POOL, "Could not attach request to pool: %s", curl_multi_strerror(rs));
		return FAILURE;
	}
}

static STATUS php_http_client_pool_curl_detach(php_http_client_pool_t *h, php_http_client_t *r)
{
	php_http_client_pool_curl_t *curl = h->ctx;
	php_http_client_curl_t *recurl = r->ctx;
	CURLMcode rs = curl_multi_remove_handle(curl->handle, recurl->handle);
	TSRMLS_FETCH_FROM_CTX(h->ts);

	if (CURLM_OK == rs) {
		return SUCCESS;
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_CLIENT_POOL, "Could not detach request from pool: %s", curl_multi_strerror(rs));
		return FAILURE;
	}
}

#ifdef PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

static STATUS php_http_client_pool_curl_wait(php_http_client_pool_t *h, struct timeval *custom_timeout)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout;
	php_http_client_pool_curl_t *curl = h->ctx;

#if PHP_HTTP_HAVE_EVENT
	if (curl->useevents) {
		TSRMLS_FETCH_FROM_CTX(h->ts);

		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented");
		return FAILURE;
	}
#endif

	if (custom_timeout && timerisset(custom_timeout)) {
		timeout = *custom_timeout;
	} else {
		long max_tout = 1000;

		if ((CURLM_OK == curl_multi_timeout(curl->handle, &max_tout)) && (max_tout > 0)) {
			timeout.tv_sec = max_tout / 1000;
			timeout.tv_usec = (max_tout % 1000) * 1000;
		} else {
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;
		}
	}

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(curl->handle, &R, &W, &E, &MAX)) {
		if (MAX == -1) {
			php_http_sleep((double) timeout.tv_sec + (double) (timeout.tv_usec / PHP_HTTP_MCROSEC));
			return SUCCESS;
		} else if (SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

static int php_http_client_pool_curl_once(php_http_client_pool_t *h)
{
	php_http_client_pool_curl_t *curl = h->ctx;

#if PHP_HTTP_HAVE_EVENT
	if (curl->useevents) {
		TSRMLS_FETCH_FROM_CTX(h->ts);
		php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "not implemented");
		return FAILURE;
	}
#endif

	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(curl->handle, &curl->unfinished));

	php_http_client_pool_curl_responsehandler(h);

	return curl->unfinished;

}
#if PHP_HTTP_HAVE_EVENT
static void dolog(int i, const char *m) {
	fprintf(stderr, "%d: %s\n", i, m);
}
#endif
static STATUS php_http_client_pool_curl_exec(php_http_client_pool_t *h)
{
	TSRMLS_FETCH_FROM_CTX(h->ts);

#if PHP_HTTP_HAVE_EVENT
	php_http_client_pool_curl_t *curl = h->ctx;

	if (curl->useevents) {
		event_set_log_callback(dolog);
		do {
#if DBG_EVENTS
			fprintf(stderr, "X");
#endif
			event_base_dispatch(PHP_HTTP_G->curl.event_base);
		} while (curl->unfinished);
	} else
#endif
	{
		while (php_http_client_pool_curl_once(h)) {
			if (SUCCESS != php_http_client_pool_curl_wait(h, NULL)) {
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

	return SUCCESS;
}

static STATUS php_http_client_pool_curl_setopt(php_http_client_pool_t *h, php_http_client_pool_setopt_opt_t opt, void *arg)
{
	php_http_client_pool_curl_t *curl = h->ctx;

	switch (opt) {
		case PHP_HTTP_CLIENT_POOL_OPT_ENABLE_PIPELINING:
			if (CURLM_OK != curl_multi_setopt(curl->handle, CURLMOPT_PIPELINING, (long) *((zend_bool *) arg))) {
				return FAILURE;
			}
			break;

		case PHP_HTTP_CLIENT_POOL_OPT_USE_EVENTS:
#if PHP_HTTP_HAVE_EVENT
			if ((curl->useevents = *((zend_bool *) arg))) {
				if (!curl->timeout) {
					curl->timeout = ecalloc(1, sizeof(struct event));
				}
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETDATA, h);
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETFUNCTION, php_http_client_pool_curl_socket_callback);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERDATA, h);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERFUNCTION, php_http_client_pool_curl_timer_callback);
			} else {
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETDATA, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_SOCKETFUNCTION, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERDATA, NULL);
				curl_multi_setopt(curl->handle, CURLMOPT_TIMERFUNCTION, NULL);
			}
			break;
#endif

		default:
			return FAILURE;
	}
	return SUCCESS;
}

static php_http_resource_factory_ops_t php_http_curlm_resource_factory_ops = {
	php_http_curlm_ctor,
	NULL,
	php_http_curlm_dtor
};

static zend_class_entry *get_class_entry(void)
{
	return php_http_client_pool_curl_class_entry;
}

static php_http_client_pool_ops_t php_http_client_pool_curl_ops = {
	&php_http_curlm_resource_factory_ops,
	php_http_client_pool_curl_init,
	NULL /* copy */,
	php_http_client_pool_curl_dtor,
	NULL /*reset */,
	php_http_client_pool_curl_exec,
	php_http_client_pool_curl_wait,
	php_http_client_pool_curl_once,
	php_http_client_pool_curl_attach,
	php_http_client_pool_curl_detach,
	php_http_client_pool_curl_setopt,
	(php_http_new_t) php_http_client_pool_curl_object_new_ex,
	get_class_entry
};

PHP_HTTP_API php_http_client_pool_ops_t *php_http_client_pool_curl_get_ops(void)
{
	return &php_http_client_pool_curl_ops;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClientCURL, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClientCURL, method, 0)
#define PHP_HTTP_CURL_ME(method, visibility)	PHP_ME(HttpClientCURL, method, PHP_HTTP_ARGS(HttpClientCURL, method), visibility)
#define PHP_HTTP_CURL_ALIAS(method, func)		PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpClientCURL, method))
#define PHP_HTTP_CURL_MALIAS(me, al, vis)		ZEND_FENTRY(me, ZEND_MN(HttpClientCURL_##al), PHP_HTTP_ARGS(HttpClientCURL, al), vis)

zend_class_entry *php_http_client_pool_curl_class_entry;
zend_function_entry php_http_client_pool_curl_method_entry[] = {
	EMPTY_FUNCTION_ENTRY
};

zend_object_value php_http_client_pool_curl_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_client_pool_curl_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_client_pool_curl_object_new_ex(zend_class_entry *ce, php_http_client_pool_t *p, php_http_client_pool_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_client_pool_object_t *o;

	o = ecalloc(1, sizeof(php_http_client_pool_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (!(o->pool = p)) {
		o->pool = php_http_client_pool_init(NULL, &php_http_client_pool_curl_ops, NULL, NULL TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_client_pool_object_free, NULL TSRMLS_CC);
	ov.handlers = php_http_client_pool_get_object_handlers();

	return ov;
}


PHP_MINIT_FUNCTION(http_client_pool_curl)
{
	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_client_pool.curl"), &php_http_curlm_resource_factory_ops, NULL, NULL)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http\\Client\\Pool, CURL, http_client_pool_curl, php_http_client_pool_class_entry, 0);
	php_http_client_pool_curl_class_entry->create_object = php_http_client_pool_curl_object_new;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_client_pool_curl)
{
#if PHP_HTTP_HAVE_EVENT
	if (!PHP_HTTP_G->curl.event_base && !(PHP_HTTP_G->curl.event_base = event_init())) {
		return FAILURE;
	}
#endif

	return SUCCESS;
}

#endif /* PHP_HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

