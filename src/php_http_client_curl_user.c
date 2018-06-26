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

#include "php_network.h"
#include "zend_closures.h"

#if PHP_HTTP_HAVE_LIBCURL

typedef struct php_http_client_curl_user_ev {
	php_stream *socket;
	php_http_client_curl_user_context_t *context;
} php_http_client_curl_user_ev_t;

static ZEND_NAMED_FUNCTION(php_http_client_curl_user_handler)
{
	zval *zstream = NULL, *zclient = NULL;
	php_stream *stream = NULL;
	long action = 0;
	php_socket_t fd = CURL_SOCKET_TIMEOUT;
	php_http_client_object_t *client = NULL;
	php_http_client_curl_t *curl;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "O|rl", &zclient, php_http_client_get_class_entry(), &zstream, &action)) {
		return;
	}

	client = PHP_HTTP_OBJ(NULL, zclient);
	if (zstream) {
		php_stream_from_zval(stream, zstream);

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
	} else {
		zval args[1], *ztimeout = &args[0];

		ZVAL_LONG(ztimeout, timeout_ms);
		php_http_object_method_call(&context->timer, &context->user, NULL, 1, args);
		zval_ptr_dtor(ztimeout);
	}
}

static int php_http_client_curl_user_socket(CURL *easy, curl_socket_t sock, int action, void *socket_data, void *assign_data)
{
	php_http_client_curl_user_context_t *ctx = socket_data;
	php_http_client_curl_t *curl = ctx->client->ctx;
	php_http_client_curl_user_ev_t *ev = assign_data;
	zval args[2], *zaction = &args[1], *zsocket = &args[0];

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
			php_stream_to_zval(ev->socket, zsocket);
			Z_TRY_ADDREF_P(zsocket);
			ZVAL_LONG(zaction, action);
			php_http_object_method_call(&ctx->socket, &ctx->user, NULL, 2, args);
			zval_ptr_dtor(zsocket);
			zval_ptr_dtor(zaction);
			break;

		default:
			php_error_docref(NULL, E_WARNING, "Unknown socket action %d", action);
			return -1;
	}

	if (action == CURL_POLL_REMOVE) {
		php_stream_close(ev->socket);
		efree(ev);
		curl_multi_assign(curl->handle->multi, sock, NULL);
	}
	return 0;
}

static ZEND_RESULT_CODE php_http_client_curl_user_once(void *context)
{
	php_http_client_curl_user_context_t *ctx = context;

#if DBG_EVENTS
	fprintf(stderr, "O");
#endif

	return php_http_object_method_call(&ctx->once, &ctx->user, NULL, 0, NULL);
}

static ZEND_RESULT_CODE php_http_client_curl_user_wait(void *context, struct timeval *custom_timeout)
{
	php_http_client_curl_user_context_t *ctx = context;
	struct timeval timeout;
	zval args[1], *ztimeout = &args[0];
	ZEND_RESULT_CODE rv;

#if DBG_EVENTS
	fprintf(stderr, "W");
#endif

	if (!custom_timeout || !timerisset(custom_timeout)) {
		php_http_client_curl_get_timeout(ctx->client->ctx, 1000, &timeout);
		custom_timeout = &timeout;
	}

	ZVAL_LONG(ztimeout, custom_timeout->tv_sec * 1000 + custom_timeout->tv_usec / 1000);
	rv = php_http_object_method_call(&ctx->wait, &ctx->user, NULL, 1, args);
	zval_ptr_dtor(ztimeout);

	return rv;
}

static ZEND_RESULT_CODE php_http_client_curl_user_exec(void *context)
{
	php_http_client_curl_user_context_t *ctx = context;
	php_http_client_curl_t *curl = ctx->client->ctx;

#if DBG_EVENTS
	fprintf(stderr, "E");
#endif

	/* kickstart */
	php_http_client_curl_loop(ctx->client, CURL_SOCKET_TIMEOUT, 0);

	do {
		if (SUCCESS != php_http_object_method_call(&ctx->send, &ctx->user, NULL, 0, NULL)) {
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
	zval args[1], *zclosure = &args[0];

#if DBG_EVENTS
	fprintf(stderr, "I");
#endif

	ctx = ecalloc(1, sizeof(*ctx));
	ctx->client = client;
	ZVAL_COPY(&ctx->user, user_data);

	memset(&ctx->closure, 0, sizeof(ctx->closure));
	ctx->closure.common.type = ZEND_INTERNAL_FUNCTION;
	ctx->closure.common.function_name = zend_string_init(ZEND_STRL("php_http_client_curl_user_handler"), 0);
	ctx->closure.internal_function.handler = php_http_client_curl_user_handler;

	zend_create_closure(zclosure, &ctx->closure, NULL, NULL, NULL);

	php_http_object_method_init(&init, &ctx->user, ZEND_STRL("init"));
	php_http_object_method_call(&init, &ctx->user, NULL, 1, args);
	php_http_object_method_dtor(&init);
	zval_ptr_dtor(zclosure);

	php_http_object_method_init(&ctx->timer, &ctx->user, ZEND_STRL("timer"));
	php_http_object_method_init(&ctx->socket, &ctx->user, ZEND_STRL("socket"));
	php_http_object_method_init(&ctx->once, &ctx->user, ZEND_STRL("once"));
	php_http_object_method_init(&ctx->wait, &ctx->user, ZEND_STRL("wait"));
	php_http_object_method_init(&ctx->send, &ctx->user, ZEND_STRL("send"));

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

	zend_string_release(ctx->closure.common.function_name);
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

static zend_class_entry *php_http_client_curl_user_class_entry;

zend_class_entry *php_http_client_curl_user_get_class_entry()
{
	return php_http_client_curl_user_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_init, 0, 0, 1)
	/* using IS_CALLABLE type hint would create a forwards compatibility break */
	ZEND_ARG_INFO(0, run)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_timer, 0, 0, 1)
#if PHP_VERSION_ID >= 70000
	ZEND_ARG_TYPE_INFO(0, timeout_ms, IS_LONG, 0)
#else
	ZEND_ARG_INFO(0, timeout_ms)
#endif
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_socket, 0, 0, 2)
	ZEND_ARG_INFO(0, socket)
#if PHP_VERSION_ID >= 70000
	ZEND_ARG_TYPE_INFO(0, action, IS_LONG, 0)
#else
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
	php_http_client_curl_user_class_entry = zend_register_internal_interface(&ce);

	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_NONE"), CURL_POLL_NONE);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_IN"), CURL_POLL_IN);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_OUT"), CURL_POLL_OUT);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_INOUT"), CURL_POLL_INOUT);
	zend_declare_class_constant_long(php_http_client_curl_user_class_entry, ZEND_STRL("POLL_REMOVE"), CURL_POLL_REMOVE);

	return SUCCESS;
}

#endif /* PHP_HTTP_HAVE_LIBCURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
