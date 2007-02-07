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

#ifndef PHP_HTTP_EXCEPTION_OBJECT_H
#define PHP_HTTP_EXCEPTION_OBJECT_H
#ifdef ZEND_ENGINE_2

#include "zend_exceptions.h"

PHP_MINIT_FUNCTION(http_exception_object);

#define HTTP_EX_DEF_CE http_exception_object_ce
#define HTTP_EX_CE(name) http_ ##name## _exception_object_ce

extern zend_class_entry *http_exception_object_ce;
extern zend_class_entry *HTTP_EX_CE(runtime);
extern zend_class_entry *HTTP_EX_CE(header);
extern zend_class_entry *HTTP_EX_CE(malformed_headers);
extern zend_class_entry *HTTP_EX_CE(request_method);
extern zend_class_entry *HTTP_EX_CE(message_type);
extern zend_class_entry *HTTP_EX_CE(invalid_param);
extern zend_class_entry *HTTP_EX_CE(encoding);
extern zend_class_entry *HTTP_EX_CE(request);
extern zend_class_entry *HTTP_EX_CE(request_pool);
extern zend_class_entry *HTTP_EX_CE(socket);
extern zend_class_entry *HTTP_EX_CE(response);
extern zend_class_entry *HTTP_EX_CE(url);
extern zend_function_entry http_exception_object_fe[];

#define http_exception_get_default _http_exception_get_default
extern zend_class_entry *_http_exception_get_default();

#define http_exception_get_for_code(c) _http_exception_get_for_code(c)
extern zend_class_entry *_http_exception_get_for_code(long code);

PHP_METHOD(HttpException, __toString);

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

