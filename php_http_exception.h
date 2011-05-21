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

extern zend_class_entry *php_http_exception_class_entry;
extern zend_function_entry php_http_exception_method_entry[];

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

