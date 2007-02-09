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

#ifndef PHP_HTTP_REQUEST_DATASHARE_API_H
#define PHP_HTTP_REQUEST_DATASHARE_API_H
#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2

#ifdef ZTS
typedef struct _http_request_datashare_lock_t {
	CURL *ch;
	MUTEX_T mx;
} http_request_datashare_lock;

typedef union _http_request_datashare_handle_t {
	zend_llist *list;
	http_request_datashare_lock *locks;
} http_request_datashare_handle;
#else
typedef struct _http_request_datashare_handle_t {
	zend_llist *list;
} http_request_datashare_handle;
#endif

typedef struct _http_request_datashare_t {
	CURLSH *ch;
	zend_bool persistent;
	http_request_datashare_handle handle;
} http_request_datashare;

#define HTTP_RSHARE_HANDLES(s) ((s)->persistent ? &HTTP_G->request.datashare.handles : (s)->handle.list)

#define http_request_datashare_global_get _http_request_datashare_global_get
extern http_request_datashare *_http_request_datashare_global_get(void);

extern PHP_MINIT_FUNCTION(http_request_datashare);
extern PHP_MSHUTDOWN_FUNCTION(http_request_datashare);
extern PHP_RINIT_FUNCTION(http_request_datashare);
extern PHP_RSHUTDOWN_FUNCTION(http_request_datashare);

#define http_request_datashare_new() _http_request_datashare_init_ex(NULL, 0 TSRMLS_CC)
#define http_request_datashare_init(s) _http_request_datashare_init_ex((s), 0 TSRMLS_CC)
#define http_request_datashare_init_ex(s, p) _http_request_datashare_init_ex((s), (p) TSRMLS_CC)
PHP_HTTP_API http_request_datashare *_http_request_datashare_init_ex(http_request_datashare *share, zend_bool persistent TSRMLS_DC);

#define http_request_datashare_attach(s, r) _http_request_datashare_attach((s), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_datashare_attach(http_request_datashare *share, zval *request TSRMLS_DC);

#define http_request_datashare_detach(s, r) _http_request_datashare_detach((s), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_datashare_detach(http_request_datashare *share, zval *request TSRMLS_DC);

#define http_request_datashare_detach_all(s) _http_request_datashare_detach_all((s) TSRMLS_CC)
PHP_HTTP_API void _http_request_datashare_detach_all(http_request_datashare *share TSRMLS_DC);

#define http_request_datashare_dtor(s) _http_request_datashare_dtor((s) TSRMLS_CC)
PHP_HTTP_API void _http_request_datashare_dtor(http_request_datashare *share TSRMLS_DC);

#define http_request_datashare_free(s) _http_request_datashare_free((s) TSRMLS_CC)
PHP_HTTP_API void _http_request_datashare_free(http_request_datashare **share TSRMLS_DC);

#define http_request_datashare_set(s, o, l, e) _http_request_datashare_set((s), (o), (l), (e) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_datashare_set(http_request_datashare *share, const char *option, size_t option_len, zend_bool enable TSRMLS_DC);


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

