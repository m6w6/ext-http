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

#ifndef PHP_HTTP_REQUEST_API_H
#define PHP_HTTP_REQUEST_API_H

#ifdef HTTP_HAVE_CURL

#include "php_http_request_body_api.h"
#include "php_http_request_method_api.h"

extern PHP_MINIT_FUNCTION(http_request);
extern PHP_MSHUTDOWN_FUNCTION(http_request);

typedef struct _http_request_t {
	CURL *ch;
	char *url;
	http_request_method meth;
	http_request_body *body;
	
	struct {
		curl_infotype last_type;
		phpstr request;
		phpstr response;
	} conv;
	
	struct {
		phpstr cookies;
		HashTable options;
		struct curl_slist *headers;
	} _cache;
	
	char _error[CURL_ERROR_SIZE+1];
	zval *_progress_callback;

#ifdef ZTS
	void ***tsrm_ls;
#endif

	uint _in_progress_cb:1;

} http_request;

#define http_curl_init(r) http_curl_init_ex(NULL, (r))
#define http_curl_init_ex(c, r) _http_curl_init_ex((c), (r) TSRMLS_CC)
PHP_HTTP_API CURL *_http_curl_init_ex(CURL *ch, http_request *request TSRMLS_DC);

#define http_curl_free(c) _http_curl_free((c) TSRMLS_CC)
PHP_HTTP_API void _http_curl_free(CURL **ch TSRMLS_DC);

#define http_request_new() _http_request_init_ex(NULL, NULL, 0, NULL ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
#define http_request_init(r) _http_request_init_ex((r), NULL, 0, NULL ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
#define http_request_init_ex(r, c, m, u) _http_request_init_ex((r), (c), (m), (u) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API http_request *_http_request_init_ex(http_request *request, CURL *ch, http_request_method meth, const char *url ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);

#define http_request_dtor(r) _http_request_dtor((r))
PHP_HTTP_API void _http_request_dtor(http_request *request);

#define http_request_free(r) _http_request_free((r))
PHP_HTTP_API void _http_request_free(http_request **request);

#define http_request_reset(r) _http_request_reset(r)
PHP_HTTP_API void _http_request_reset(http_request *r);

#define http_request_enable_cookies(r) _http_request_enable_cookies(r)
PHP_HTTP_API STATUS _http_request_enable_cookies(http_request *request);

#define http_request_reset_cookies(r, s) _http_request_reset_cookies((r), (s))
PHP_HTTP_API STATUS _http_request_reset_cookies(http_request *request, int session_only);

#define http_request_defaults(r) _http_request_defaults(r)
PHP_HTTP_API void _http_request_defaults(http_request *request);

#define http_request_prepare(r, o) _http_request_prepare((r), (o))
PHP_HTTP_API STATUS _http_request_prepare(http_request *request, HashTable *options);

#define http_request_exec(r) _http_request_exec((r))
PHP_HTTP_API void _http_request_exec(http_request *request);

#define http_request_info(r, i) _http_request_info((r), (i))
PHP_HTTP_API void _http_request_info(http_request *request, HashTable *info);

#define http_request_set_progress_callback(r, cb) _http_request_set_progress_callback((r), (cb))
PHP_HTTP_API void _http_request_set_progress_callback(http_request *request, zval *cb);

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

