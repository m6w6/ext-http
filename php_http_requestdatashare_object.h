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

#ifndef PHP_HTTP_REQUEST_DATASHARE_OBJECT_H
#define PHP_HTTP_REQUEST_DATASHARE_OBJECT_H
#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2

typedef struct _http_requestdatashare_object_t {
	zend_object zo;
	http_request_datashare *share;
} http_requestdatashare_object;

extern zend_class_entry *http_requestdatashare_object_ce;
extern zend_function_entry http_requestdatashare_object_fe[];

extern PHP_MINIT_FUNCTION(http_requestdatashare_object);

#define http_requestdatashare_object_new(ce) _http_requestdatashare_object_new((ce) TSRMLS_CC)
extern zend_object_value _http_requestdatashare_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_requestdatashare_object_new_ex(ce, s, ptr) _http_requestdatashare_object_new_ex((ce), (s), (ptr) TSRMLS_CC)
extern zend_object_value _http_requestdatashare_object_new_ex(zend_class_entry *ce, http_request_datashare *share, http_requestdatashare_object **ptr TSRMLS_DC);
#define http_requestdatashare_object_free(o) _http_requestdatashare_object_free((o) TSRMLS_CC)
extern void _http_requestdatashare_object_free(zend_object *object TSRMLS_DC);

PHP_METHOD(HttpRequestDataShare, __destruct);
PHP_METHOD(HttpRequestDataShare, count);
PHP_METHOD(HttpRequestDataShare, attach);
PHP_METHOD(HttpRequestDataShare, detach);
PHP_METHOD(HttpRequestDataShare, reset);
PHP_METHOD(HttpRequestDataShare, factory);
#ifndef WONKY
PHP_METHOD(HttpRequestDataShare, singleton);
#endif

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

