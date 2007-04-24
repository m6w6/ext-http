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

#ifndef PHP_HTTP_REQUEST_METHOD_API_H
#define PHP_HTTP_REQUEST_METHOD_API_H

typedef enum _http_request_method_t {
	/* force the enum to be signed */
	HTTP_NEG_REQUEST_METHOD	=-1,
	HTTP_NO_REQUEST_METHOD	= 0,
	/* HTTP/1.1 */
	HTTP_GET				= 1,
	HTTP_HEAD				= 2,
	HTTP_POST				= 3,
	HTTP_PUT				= 4,
	HTTP_DELETE				= 5,
	HTTP_OPTIONS			= 6,
	HTTP_TRACE				= 7,
	HTTP_CONNECT			= 8,
	/* WebDAV - RFC 2518 */
	HTTP_PROPFIND			= 9,
	HTTP_PROPPATCH			= 10,
	HTTP_MKCOL				= 11,
	HTTP_COPY				= 12,
	HTTP_MOVE				= 13,
	HTTP_LOCK				= 14,
	HTTP_UNLOCK				= 15,
	/* WebDAV Versioning - RFC 3253 */
	HTTP_VERSION_CONTROL	= 16,
	HTTP_REPORT				= 17,
	HTTP_CHECKOUT			= 18,
	HTTP_CHECKIN			= 19,
	HTTP_UNCHECKOUT			= 20,
	HTTP_MKWORKSPACE		= 21,
	HTTP_UPDATE				= 22,
	HTTP_LABEL				= 23,
	HTTP_MERGE				= 24,
	HTTP_BASELINE_CONTROL	= 25,
	HTTP_MKACTIVITY			= 26,
	/* WebDAV Access Control - RFC 3744 */
	HTTP_ACL				= 27,
	HTTP_MAX_REQUEST_METHOD	= 28
} http_request_method;

#define HTTP_MIN_REQUEST_METHOD (HTTP_NO_REQUEST_METHOD + 1)
#define HTTP_STD_REQUEST_METHOD(m) ((m > HTTP_NO_REQUEST_METHOD) && (m < HTTP_MAX_REQUEST_METHOD))

extern PHP_MINIT_FUNCTION(http_request_method);
extern PHP_RINIT_FUNCTION(http_request_method);
extern PHP_RSHUTDOWN_FUNCTION(http_request_method);

#define http_request_method_name(m) _http_request_method_name((m) TSRMLS_CC)
PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC);

#define http_request_method_exists(u, l, c) _http_request_method_exists((u), (l), (c) TSRMLS_CC)
PHP_HTTP_API int _http_request_method_exists(int by_name, http_request_method id, const char *name TSRMLS_DC);

#define http_request_method_register(m, l) _http_request_method_register((m), (l) TSRMLS_CC)
PHP_HTTP_API int _http_request_method_register(const char *method, int method_name_len TSRMLS_DC);

#define http_request_method_unregister(mn) _http_request_method_unregister((mn) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_method_unregister(int method TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

