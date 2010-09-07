/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: php_http_exception_object.h 292841 2009-12-31 08:48:57Z mike $ */

#ifndef PHP_HTTP_EXCEPTION_H
#define PHP_HTTP_EXCEPTION_H

PHP_MINIT_FUNCTION(http_exception_object);

#define PHP_HTTP_EX_DEF_CE php_http_exception_class_entry
#define PHP_HTTP_EX_CE(name) php_http_ ##name## _exception_class_entry

extern zend_class_entry *PHP_HTTP_EX_DEF_CE;
extern zend_class_entry *PHP_HTTP_EX_CE(runtime);
extern zend_class_entry *PHP_HTTP_EX_CE(header);
extern zend_class_entry *PHP_HTTP_EX_CE(malformed_headers);
extern zend_class_entry *PHP_HTTP_EX_CE(request_method);
extern zend_class_entry *PHP_HTTP_EX_CE(message);
extern zend_class_entry *PHP_HTTP_EX_CE(message_type);
extern zend_class_entry *PHP_HTTP_EX_CE(invalid_param);
extern zend_class_entry *PHP_HTTP_EX_CE(encoding);
extern zend_class_entry *PHP_HTTP_EX_CE(request);
extern zend_class_entry *PHP_HTTP_EX_CE(request_pool);
extern zend_class_entry *PHP_HTTP_EX_CE(socket);
extern zend_class_entry *PHP_HTTP_EX_CE(response);
extern zend_class_entry *PHP_HTTP_EX_CE(url);
extern zend_class_entry *PHP_HTTP_EX_CE(querystring);
extern zend_class_entry *PHP_HTTP_EX_CE(cookie);
extern zend_function_entry php_http_exception_method_entry[];

PHP_HTTP_API zend_class_entry *php_http_exception_get_default(void);
PHP_HTTP_API zend_class_entry *php_http_exception_get_for_code(long code);

PHP_MINIT_FUNCTION(http_exception);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

