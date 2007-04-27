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

#ifndef PHP_HTTP_REQUEST_POOL_API_H
#define PHP_HTTP_REQUEST_POOL_API_H
#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2

typedef struct _http_request_pool_t {
	CURLM *ch;
	zend_llist finished;
	zend_llist handles;
	int unfinished;
#ifdef ZTS
	void ***tsrm_ls;
#endif
} http_request_pool;

typedef int (*http_request_pool_apply_func)(http_request_pool *pool, zval *request);
typedef int (*http_request_pool_apply_with_arg_func)(http_request_pool *pool, zval *request, void *arg);

PHP_MINIT_FUNCTION(http_request_pool);
#ifdef HTTP_HAVE_EVENT
PHP_RINIT_FUNCTION(http_request_pool);
#endif

#define http_request_pool_responsehandler(p, r, c) _http_request_pool_responsehandler((p), (r), (c))
extern int _http_request_pool_responsehandler(http_request_pool *pool, zval *req, void *ch);

#define http_request_pool_init(p) _http_request_pool_init((p) TSRMLS_CC)
PHP_HTTP_API http_request_pool *_http_request_pool_init(http_request_pool *pool TSRMLS_DC);

#define http_request_pool_attach(p, r) _http_request_pool_attach((p), (r))
PHP_HTTP_API STATUS _http_request_pool_attach(http_request_pool *pool, zval *request);

#define http_request_pool_detach(p, r) _http_request_pool_detach((p), (r))
PHP_HTTP_API STATUS _http_request_pool_detach(http_request_pool *pool, zval *request);

#define http_request_pool_apply(p, f) _http_request_pool_apply((p), (f))
PHP_HTTP_API void _http_request_pool_apply(http_request_pool *pool, http_request_pool_apply_func cb);

#define http_request_pool_apply_with_arg(p, f, a) _http_request_pool_apply_with_arg((p), (f), (a))
PHP_HTTP_API void _http_request_pool_apply_with_arg(http_request_pool *pool, http_request_pool_apply_with_arg_func cb, void *arg);

#define http_request_pool_detach_all(p) _http_request_pool_detach_all((p))
PHP_HTTP_API void _http_request_pool_detach_all(http_request_pool *pool);

#define http_request_pool_send(p) _http_request_pool_send((p))
PHP_HTTP_API STATUS _http_request_pool_send(http_request_pool *pool);

#define http_request_pool_select _http_request_pool_select
PHP_HTTP_API STATUS _http_request_pool_select(http_request_pool *pool);

#define http_request_pool_perform(p) _http_request_pool_perform((p))
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool);

#define http_request_pool_dtor(p) _http_request_pool_dtor((p))
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool);

#endif
#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

