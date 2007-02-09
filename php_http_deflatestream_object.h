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

#ifndef PHP_HTTP_DEFLATESTREAM_OBJECT_H
#define PHP_HTTP_DEFLATESTREAM_OBJECT_H
#ifdef HTTP_HAVE_ZLIB
#ifdef ZEND_ENGINE_2

typedef struct _http_deflatestream_object_t {
	zend_object zo;
	http_encoding_stream *stream;
} http_deflatestream_object;

extern zend_class_entry *http_deflatestream_object_ce;
extern zend_function_entry http_deflatestream_object_fe[];

extern PHP_MINIT_FUNCTION(http_deflatestream_object);

#define http_deflatestream_object_new(ce) _http_deflatestream_object_new((ce) TSRMLS_CC)
extern zend_object_value _http_deflatestream_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_deflatestream_object_new_ex(ce, s, ptr) _http_deflatestream_object_new_ex((ce), (s), (ptr) TSRMLS_CC)
extern zend_object_value _http_deflatestream_object_new_ex(zend_class_entry *ce, http_encoding_stream *s, http_deflatestream_object **ptr TSRMLS_DC);
#define http_deflatestream_object_clone(zobj) _http_deflatestream_object_clone_obj(zobj TSRMLS_CC)
extern zend_object_value _http_deflatestream_object_clone_obj(zval *object TSRMLS_DC);
#define http_deflatestream_object_free(o) _http_deflatestream_object_free((o) TSRMLS_CC)
extern void _http_deflatestream_object_free(zend_object *object TSRMLS_DC);

PHP_METHOD(HttpDeflateStream, __construct);
PHP_METHOD(HttpDeflateStream, factory);
PHP_METHOD(HttpDeflateStream, update);
PHP_METHOD(HttpDeflateStream, flush);
PHP_METHOD(HttpDeflateStream, finish);

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

