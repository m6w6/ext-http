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

#ifndef PHP_HTTP_REQUEST_METHOD_H
#define PHP_HTTP_REQUEST_METHOD_H

typedef enum php_http_request_method {
	/* force the enum to be signed */
	PHP_HTTP_NEG_REQUEST_METHOD	=-1,
	PHP_HTTP_NO_REQUEST_METHOD	= 0,
	/* HTTP/1.1 */
	PHP_HTTP_GET				= 1,
	PHP_HTTP_HEAD				= 2,
	PHP_HTTP_POST				= 3,
	PHP_HTTP_PUT				= 4,
	PHP_HTTP_DELETE				= 5,
	PHP_HTTP_OPTIONS			= 6,
	PHP_HTTP_TRACE				= 7,
	PHP_HTTP_CONNECT			= 8,
	/* WebDAV - RFC 2518 */
	PHP_HTTP_PROPFIND			= 9,
	PHP_HTTP_PROPPATCH			= 10,
	PHP_HTTP_MKCOL				= 11,
	PHP_HTTP_COPY				= 12,
	PHP_HTTP_MOVE				= 13,
	PHP_HTTP_LOCK				= 14,
	PHP_HTTP_UNLOCK				= 15,
	/* WebDAV Versioning - RFC 3253 */
	PHP_HTTP_VERSION_CONTROL	= 16,
	PHP_HTTP_REPORT				= 17,
	PHP_HTTP_CHECKOUT			= 18,
	PHP_HTTP_CHECKIN			= 19,
	PHP_HTTP_UNCHECKOUT			= 20,
	PHP_HTTP_MKWORKSPACE		= 21,
	PHP_HTTP_UPDATE				= 22,
	PHP_HTTP_LABEL				= 23,
	PHP_HTTP_MERGE				= 24,
	PHP_HTTP_BASELINE_CONTROL	= 25,
	PHP_HTTP_MKACTIVITY			= 26,
	/* WebDAV Access Control - RFC 3744 */
	PHP_HTTP_ACL				= 27,
	PHP_HTTP_MAX_REQUEST_METHOD	= 28
} php_http_request_method_t;

PHP_HTTP_API const char *php_http_request_method_name(php_http_request_method_t meth TSRMLS_DC);
PHP_HTTP_API STATUS php_http_request_method_register(const char *meth_str, size_t meth_len, long *id TSRMLS_DC);

extern zend_class_entry *php_http_request_method_class_entry;
extern zend_function_entry php_http_request_method_method_entry[];

extern PHP_METHOD(HttpRequestMethod, __construct);
extern PHP_METHOD(HttpRequestMethod, __toString);
extern PHP_METHOD(HttpRequestMethod, getId);

extern PHP_METHOD(HttpRequestMethod, exists);
extern PHP_METHOD(HttpRequestMethod, register);

extern PHP_MINIT_FUNCTION(http_request_method);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

