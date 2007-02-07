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

#ifndef PHP_HTTP_REQUEST_BODY_API_H
#define PHP_HTTP_REQUEST_BODY_API_H

#ifdef HTTP_HAVE_CURL

#define HTTP_REQUEST_BODY_EMPTY			0
#define HTTP_REQUEST_BODY_CSTRING		1
#define HTTP_REQUEST_BODY_CURLPOST		2
#define HTTP_REQUEST_BODY_UPLOADFILE	3
typedef struct _http_request_body_t {
	void *data;
	size_t size;
	uint type:3;
	uint free:1;
	uint priv:28;
} http_request_body;


#define http_request_body_new() http_request_body_init(NULL)
#define http_request_body_init(b) http_request_body_init_ex((b), 0, NULL, 0, 0)
#define http_request_body_init_ex(b, t, d, l, f) _http_request_body_init_ex((b), (t), (d), (l), (f) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
#define http_request_body_init_rel(b, t, d, l, f) _http_request_body_init_ex((b), (t), (d), (l), (f) ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC TSRMLS_CC)
PHP_HTTP_API http_request_body *_http_request_body_init_ex(http_request_body *body, int type, void *data, size_t len, zend_bool free ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);

#define http_request_body_fill(b, fields, files) _http_request_body_fill((b), (fields), (files) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API http_request_body *_http_request_body_fill(http_request_body *body, HashTable *fields, HashTable *files ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);

#define http_request_body_encode(b, s, l) _http_request_body_encode((b), (s), (l) TSRMLS_CC)
PHP_HTTP_API STATUS  _http_request_body_encode(http_request_body *body, char **buf, size_t *len TSRMLS_DC);

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

