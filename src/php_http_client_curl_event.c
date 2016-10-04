/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

#if PHP_HTTP_HAVE_LIBCURL
#if PHP_HTTP_HAVE_LIBEVENT

#include <event.h>
#ifndef DBG_EVENTS
#	define DBG_EVENTS 0
#endif

typedef struct php_http_client_curl_event_context {
	php_http_client_t *client;
	struct event_base *evbase;
	struct event *timeout;
} php_http_client_curl_event_context_t;

typedef struct php_http_client_curl_event_ev {
	struct event evnt;
	php_http_client_curl_event_context_t *context;
} php_http_client_curl_event_ev_t;

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

static void php_http_client_curl_event_handler(void *context, curl_socket_t s, int curl_action)
{
	CURLMcode rc;
	php_http_client_curl_event_context_t *ctx = context;
	php_http_client_curl_t *curl = ctx->client->ctx;

#if DBG_EVENTS
	fprintf(stderr, "H");
#endif

	do {
		rc = curl_multi_socket_action(curl->handle->multi, s, curl_action, &curl->unfinished);
	} while (CURLM_CALL_MULTI_PERFORM == rc);

	if (CURLM_OK != rc) {
		php_error_docref(NULL, E_WARNING, "%s",  curl_multi_strerror(rc));
	}

	php_http_client_curl_responsehandler(ctx->client);
}

static void php_http_client_curl_event_timeout_callback(int socket, short action, void *event_data)
{
#if DBG_EVENTS
	fprintf(stderr, "T");
#endif

	/* ignore and use -1,0 on timeout */
	(void) socket;
	(void) action;

	php_http_client_curl_event_handler(event_data, CURL_SOCKET_TIMEOUT, 0);
}

static void php_http_client_curl_event_timer(CURLM *multi, long timeout_ms, void *timer_data)
{
	php_http_client_curl_event_context_t *context = timer_data;
	struct timeval timeout;

#if DBG_EVENTS
	fprintf(stderr, "(%ld)", timeout_ms);
#endif

	switch (timeout_ms) {
	case -1:
		if (event_initialized(context->timeout) && event_pending(context->timeout, EV_TIMEOUT, NULL)) {
			event_del(context->timeout);
		}
		break;
	case 0:
		php_http_client_curl_event_handler(context, CURL_SOCKET_TIMEOUT, 0);
		break;
	default:
		if (!event_initialized(context->timeout)) {
			event_assign(context->timeout, context->evbase, CURL_SOCKET_TIMEOUT, 0, php_http_client_curl_event_timeout_callback, context);
		}

		timeout.tv_sec = timeout_ms / 1000;
		timeout.tv_usec = (timeout_ms % 1000) * 1000;

		if (!event_pending(context->timeout, EV_TIMEOUT, &timeout)) {
			event_add(context->timeout, &timeout);
		}
		break;
	}
}

static void php_http_client_curl_event_callback(int socket, short action, void *event_data)
{
	php_http_client_curl_event_context_t *ctx = event_data;
	php_http_client_curl_t *curl = ctx->client->ctx;

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif

	php_http_client_curl_event_handler(event_data, socket, etoca(action));

	/* remove timeout if there are no transfers left */
	if (!curl->unfinished && event_initialized(ctx->timeout) && event_pending(ctx->timeout, EV_TIMEOUT, NULL)) {
		event_del(ctx->timeout);
	}
}

static int php_http_client_curl_event_socket(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_client_curl_event_context_t *ctx = socket_data;
	php_http_client_curl_t *curl = ctx->client->ctx;
	int events = EV_PERSIST;
	php_http_client_curl_event_ev_t *ev = assign_data;

#if DBG_EVENTS
	fprintf(stderr, "S");
#endif

	if (!ev) {
		ev = ecalloc(1, sizeof(*ev));
		ev->context = ctx;
		curl_multi_assign(curl->handle->multi, sock, ev);
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
			php_error_docref(NULL, E_WARNING, "Unknown socket action %d", action);
			return -1;
	}

	event_assign(&ev->evnt, ctx->evbase, sock, events, php_http_client_curl_event_callback, ctx);
	event_add(&ev->evnt, NULL);

	return 0;
}

static ZEND_RESULT_CODE php_http_client_curl_event_once(void *context)
{
	php_http_client_curl_event_context_t *ctx = context;

#if DBG_EVENTS
	fprintf(stderr, "O");
#endif

	if (0 > event_base_loop(ctx->evbase, EVLOOP_NONBLOCK)) {
		return FAILURE;
	}
	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_client_curl_event_wait(void *context, struct timeval *custom_timeout)
{
	php_http_client_curl_event_context_t *ctx = context;
	struct timeval timeout;

#if DBG_EVENTS
	fprintf(stderr, "W");
#endif

	if (!event_initialized(ctx->timeout)) {
		if (0 > event_assign(ctx->timeout, ctx->evbase, CURL_SOCKET_TIMEOUT, 0, php_http_client_curl_event_timeout_callback, ctx)) {
			return FAILURE;
		}
	} else if (custom_timeout && timerisset(custom_timeout)) {
		if (0 > event_add(ctx->timeout, custom_timeout)) {
			return FAILURE;
		}
	} else if (!event_pending(ctx->timeout, EV_TIMEOUT, NULL)) {
		php_http_client_curl_get_timeout(ctx->client->ctx, 1000, &timeout);
		if (0 > event_add(ctx->timeout, &timeout)) {
			return FAILURE;
		}
	}

	if (0 > event_base_loop(ctx->evbase, EVLOOP_ONCE)) {
		return FAILURE;
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE php_http_client_curl_event_exec(void *context)
{
	php_http_client_curl_event_context_t *ctx = context;
	php_http_client_curl_t *curl = ctx->client->ctx;

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif

	/* kickstart */
	php_http_client_curl_event_handler(ctx, CURL_SOCKET_TIMEOUT, 0);

	do {
		if (0 > event_base_dispatch(ctx->evbase)) {
			return FAILURE;
		}
	} while (curl->unfinished && !EG(exception));

	return SUCCESS;
}

static void *php_http_client_curl_event_init(php_http_client_t *client)
{
	php_http_client_curl_t *curl = client->ctx;
	php_http_client_curl_event_context_t *ctx;
	struct event_base *evb = event_base_new();

#if DBG_EVENTS
	fprintf(stderr, "I");
#endif

	if (!evb) {
		return NULL;
	}

	ctx = ecalloc(1, sizeof(*ctx));
	ctx->client = client;
	ctx->evbase = evb;
	ctx->timeout = ecalloc(1, sizeof(struct event));

	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETDATA, ctx);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETFUNCTION, php_http_client_curl_event_socket);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERDATA, ctx);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERFUNCTION, php_http_client_curl_event_timer);

	return ctx;
}

static void php_http_client_curl_event_dtor(void **context)
{
	php_http_client_curl_event_context_t *ctx = *context;
	php_http_client_curl_t *curl;

#if DBG_EVENTS
	fprintf(stderr, "D");
#endif

	curl = ctx->client->ctx;

	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETDATA, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETFUNCTION, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERDATA, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERFUNCTION, NULL);

	if (event_initialized(ctx->timeout) && event_pending(ctx->timeout, EV_TIMEOUT, NULL)) {
		event_del(ctx->timeout);
	}
	efree(ctx->timeout);
	event_base_free(ctx->evbase);

	efree(ctx);
	*context = NULL;
}

static php_http_client_curl_ops_t php_http_client_curl_event_ops = {
	&php_http_client_curl_event_init,
	&php_http_client_curl_event_dtor,
	&php_http_client_curl_event_once,
	&php_http_client_curl_event_wait,
	&php_http_client_curl_event_exec,
};

php_http_client_curl_ops_t *php_http_client_curl_event_ops_get()
{
	return &php_http_client_curl_event_ops;
}

#endif /* PHP_HTTP_HAVE_LIBEVENT */
#endif /* PHP_HTTP_HAVE_LIBCURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
