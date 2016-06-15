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
#include "php_http_client.h"
#include "php_http_client_curl.h"
#include "php_http_client_curl_user.h"

#include "php_network.h"
#include "zend_closures.h"

#if PHP_HTTP_HAVE_CURL

typedef struct php_http_client_curl_user_context {
	php_http_client_t *client;
	zval *user;
	zend_function closure;
	php_http_object_method_t timer;
	php_http_object_method_t socket;
	php_http_object_method_t once;
	php_http_object_method_t wait;
	php_http_object_method_t send;
} php_http_client_curl_user_context_t;

typedef struct php_http_client_curl_user_ev {
	php_stream *socket;
	php_http_client_curl_user_context_t *context;
} php_http_client_curl_user_ev_t;

static void php_http_client_curl_user_handler(INTERNAL_FUNCTION_PARAMETERS)
{
	zval *zstream = NULL, *zclient = NULL;
	php_stream *stream = NULL;
	long action = 0;
	php_socket_t fd = CURL_SOCKET_TIMEOUT;
	php_http_client_object_t *client = NULL;
	php_http_client_curl_t *curl;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|rl", &zclient, php_http_client_class_entry, &zstream, &action)) {
		return;
	}

	client = zend_object_store_get_object(zclient TSRMLS_CC);
	if (zstream) {
		php_stream_from_zval(stream, &zstream);

		if (SUCCESS != php_stream_cast(stream, PHP_STREAM_AS_SOCKETD, (void *) &fd, 1)) {
			return;
		}
	}
	php_http_client_curl_loop(client->client, fd, action);

	curl = client->client->ctx;
	RETVAL_LONG(curl->unfinished);
}

static void php_http_client_curl_user_timer(CURLM *multi, long timeout_ms, void *timer_data)
{
	php_http_client_curl_user_context_t *context = timer_data;

#if DBG_EVENTS
	fprintf(stderr, "\ntimer <- timeout_ms: %ld\n", timeout_ms);
#endif

	if (timeout_ms <= 0) {
		php_http_client_curl_loop(context->client, CURL_SOCKET_TIMEOUT, 0);
	} else if (timeout_ms > 0) {
		zval **args[1], *ztimeout;
		TSRMLS_FETCH_FROM_CTX(context->client->ts);

		MAKE_STD_ZVAL(ztimeout);
		ZVAL_LONG(ztimeout, timeout_ms);
		args[0] = &ztimeout;
		php_http_object_method_call(&context->timer, context->user, NULL, 1, args TSRMLS_CC);
		zval_ptr_dtor(&ztimeout);
	}
}

static int php_http_client_curl_user_socket(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_client_curl_user_context_t *ctx = socket_data;
	php_http_client_curl_t *curl = ctx->client->ctx;
	php_http_client_curl_user_ev_t *ev = assign_data;
	zval **args[2], *zaction, *zsocket;
	TSRMLS_FETCH_FROM_CTX(ctx->client->ts);

#if DBG_EVENTS
	fprintf(stderr, "S");
#endif

	if (!ev) {
		ev = ecalloc(1, sizeof(*ev));
		ev->context = ctx;
		ev->socket = php_stream_sock_open_from_socket(sock, NULL);

		curl_multi_assign(curl->handle->multi, sock, ev);
	}

	switch (action) {
		case CURL_POLL_IN:
		case CURL_POLL_OUT:
		case CURL_POLL_INOUT:
		case CURL_POLL_REMOVE:
		case CURL_POLL_NONE:
			MAKE_STD_ZVAL(zsocket);
			php_stream_to_zval(ev->socket, zsocket);
			args[0] = &zsocket;
			MAKE_STD_ZVAL(zaction);
			ZVAL_LONG(zaction, action);
			args[1] = &zaction;
			php_http_object_method_call(&ctx->socket, ctx->user, NULL, 2, args TSRMLS_CC);
			zval_ptr_dtor(&zsocket);
			zval_ptr_dtor(&zaction);
			break;

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown socket action %d", action);
			return -1;
	}

	if (action == CURL_POLL_REMOVE && ev) {
		efree(ev);
	}
	return 0;
}

static ZEND_RESULT_CODE php_http_client_curl_user_once(void *context)
{
	php_http_client_curl_user_context_t *ctx = context;
	TSRMLS_FETCH_FROM_CTX(ctx->client->ts);

#if DBG_EVENTS
	fprintf(stderr, "O");
#endif

	return php_http_object_method_call(&ctx->once, ctx->user, NULL, 0, NULL TSRMLS_CC);
}

static ZEND_RESULT_CODE php_http_client_curl_user_wait(void *context, struct timeval *custom_timeout)
{
	php_http_client_curl_user_context_t *ctx = context;
	struct timeval timeout;
	zval **args[1], *ztimeout;
	ZEND_RESULT_CODE rv;
	TSRMLS_FETCH_FROM_CTX(ctx->client->ts);

#if DBG_EVENTS
	fprintf(stderr, "W");
#endif

	if (!custom_timeout || !timerisset(custom_timeout)) {
		php_http_client_curl_get_timeout(ctx->client->ctx, 1000, &timeout);
		custom_timeout = &timeout;
	}

	MAKE_STD_ZVAL(ztimeout);
	ZVAL_LONG(ztimeout, custom_timeout->tv_sec * 1000 + custom_timeout->tv_usec / 1000);
	args[0] = &ztimeout;
	rv = php_http_object_method_call(&ctx->wait, ctx->user, NULL, 1, args TSRMLS_CC);
	zval_ptr_dtor(&ztimeout);

	return rv;
}

static ZEND_RESULT_CODE php_http_client_curl_user_exec(void *context)
{
	php_http_client_curl_user_context_t *ctx = context;
	php_http_client_curl_t *curl = ctx->client->ctx;
	TSRMLS_FETCH_FROM_CTX(ctx->client->ts);

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif

	/* kickstart */
	php_http_client_curl_loop(ctx->client, CURL_SOCKET_TIMEOUT, 0);

	do {
		if (SUCCESS != php_http_object_method_call(&ctx->send, ctx->user, NULL, 0, NULL TSRMLS_CC)) {
			return FAILURE;
		}
	} while (curl->unfinished && !EG(exception));

	return SUCCESS;
}

static void *php_http_client_curl_user_init(php_http_client_t *client, void *user_data)
{
	php_http_client_curl_t *curl = client->ctx;
	php_http_client_curl_user_context_t *ctx;
	php_http_object_method_t init;
	zval *zclosure, **args[1];
	TSRMLS_FETCH_FROM_CTX(client->ts);

#if DBG_EVENTS
	fprintf(stderr, "I");
#endif

	ctx = ecalloc(1, sizeof(*ctx));
	ctx->client = client;
	ctx->user = user_data;
	Z_ADDREF_P(ctx->user);

	memset(&ctx->closure, 0, sizeof(ctx->closure));
	ctx->closure.common.type = ZEND_INTERNAL_FUNCTION;
	ctx->closure.common.function_name = "php_http_client_curl_user_handler";
	ctx->closure.internal_function.handler = php_http_client_curl_user_handler;

	MAKE_STD_ZVAL(zclosure);
	zend_create_closure(zclosure, &ctx->closure, NULL, NULL TSRMLS_CC);
	args[0] = &zclosure;

	php_http_object_method_init(&init, ctx->user, ZEND_STRL("init") TSRMLS_CC);
	php_http_object_method_call(&init, ctx->user, NULL, 1, args TSRMLS_CC);
	php_http_object_method_dtor(&init);
	zval_ptr_dtor(&zclosure);

	php_http_object_method_init(&ctx->timer, ctx->user, ZEND_STRL("timer") TSRMLS_CC);
	php_http_object_method_init(&ctx->socket, ctx->user, ZEND_STRL("socket") TSRMLS_CC);
	php_http_object_method_init(&ctx->once, ctx->user, ZEND_STRL("once") TSRMLS_CC);
	php_http_object_method_init(&ctx->wait, ctx->user, ZEND_STRL("wait") TSRMLS_CC);
	php_http_object_method_init(&ctx->send, ctx->user, ZEND_STRL("send") TSRMLS_CC);

	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETDATA, ctx);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETFUNCTION, php_http_client_curl_user_socket);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERDATA, ctx);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERFUNCTION, php_http_client_curl_user_timer);

	return ctx;
}

static void php_http_client_curl_user_dtor(void **context)
{
	php_http_client_curl_user_context_t *ctx = *context;
	php_http_client_curl_t *curl;

#if DBG_EVENTS
	fprintf(stderr, "D");
#endif

	curl = ctx->client->ctx;

	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETDATA, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_SOCKETFUNCTION, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERDATA, NULL);
	curl_multi_setopt(curl->handle->multi, CURLMOPT_TIMERFUNCTION, NULL);

	php_http_object_method_dtor(&ctx->timer);
	php_http_object_method_dtor(&ctx->socket);
	php_http_object_method_dtor(&ctx->once);
	php_http_object_method_dtor(&ctx->wait);
	php_http_object_method_dtor(&ctx->send);

	zval_ptr_dtor(&ctx->user);

	efree(ctx);
	*context = NULL;
}

static php_http_client_curl_ops_t php_http_client_curl_user_ops = {
	&php_http_client_curl_user_init,
	&php_http_client_curl_user_dtor,
	&php_http_client_curl_user_once,
	&php_http_client_curl_user_wait,
	&php_http_client_curl_user_exec,
};

php_http_client_curl_ops_t *php_http_client_curl_user_ops_get()
{
	return &php_http_client_curl_user_ops;
}

zend_class_entry *php_http_client_curl_user_class_entry;

ZEND_BEGIN_ARG_INFO_EX(ai_init, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, run, IS_CALLABLE, 0)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_timer, 0, 0, 1)
#if PHP_VERSION_ID >= 70000
	ZEND_ARG_TYPE_INFO(0, timeout_ms, IS_LONG, 0)
#else
	ZEND_ARG_INFO(0, timeout_ms)
#endif
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_socket, 0, 0, 2)
#if PHP_VERSION_ID >= 70000
	ZEND_ARG_TYPE_INFO(0, socket, IS_RESOURCE, 0)
	ZEND_ARG_TYPE_INFO(0, action, IS_LONG, 0)
#else
	ZEND_ARG_INFO(0, socket)
	ZEND_ARG_INFO(0, action)
#endif
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_once, 0, 0, 0)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_wait, 0, 0, 0)
#if PHP_VERSION_ID >= 70000
	ZEND_ARG_TYPE_INFO(0, timeout_ms, IS_LONG, 0)
#else
	ZEND_ARG_INFO(0, timeout_ms)
#endif
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_send, 0, 0, 0)
ZEND_END_ARG_INFO();

static zend_function_entry php_http_client_curl_user_methods[] = {
	PHP_ABSTRACT_ME(HttpClientCurlUser, init, ai_init)
	PHP_ABSTRACT_ME(HttpClientCurlUser, timer, ai_timer)
	PHP_ABSTRACT_ME(HttpClientCurlUser, socket, ai_socket)
	PHP_ABSTRACT_ME(HttpClientCurlUser, once, ai_once)
	PHP_ABSTRACT_ME(HttpClientCurlUser, wait, ai_wait)
	PHP_ABSTRACT_ME(HttpClientCulrUser, send, ai_send)
	{0}
};

PHP_MINIT_FUNCTION(http_client_curl_user)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Client\\Curl", "User", php_http_client_curl_user_methods);
	php_http_client_curl_user_class_entry = zend_register_internal_interface(&ce TSRMLS_CC);

	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_NONE"), CURL_POLL_NONE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_IN"), CURL_POLL_IN TSRMLS_CC);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_OUT"), CURL_POLL_OUT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_INOUT"), CURL_POLL_INOUT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_REMOVE"), CURL_POLL_REMOVE TSRMLS_CC);

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
