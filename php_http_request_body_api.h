/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_REQUEST_BODY_API_H
#define PHP_HTTP_REQUEST_BODY_API_H

#ifdef HTTP_HAVE_CURL

#define HTTP_REQUEST_BODY_CSTRING		1
#define HTTP_REQUEST_BODY_CURLPOST		2
#define HTTP_REQUEST_BODY_UPLOADFILE	3
typedef struct {
	int type;
	void *data;
	size_t size;
} http_request_body;

#define http_request_body_new() _http_request_body_new(TSRMLS_C)
PHP_HTTP_API http_request_body *_http_request_body_new(TSRMLS_D);

#define http_request_body_fill(b, fields, files) _http_request_body_fill((b), (fields), (files) TSRMLS_CC)
PHP_HTTP_API STATUS _http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files TSRMLS_DC);

#define http_request_body_dtor(b) _http_request_body_dtor((b) TSRMLS_CC)
PHP_HTTP_API void _http_request_body_dtor(http_request_body *body TSRMLS_DC);

#define http_request_body_free(b) _http_request_body_free((b) TSRMLS_CC)
PHP_HTTP_API void _http_request_body_free(http_request_body **body TSRMLS_DC);

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

