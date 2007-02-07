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

#ifndef PHP_HTTP_UTIL_OBJECT_H
#define PHP_HTTP_UTIL_OBJECT_H
#ifdef ZEND_ENGINE_2

extern zend_class_entry *http_util_object_ce;
extern zend_function_entry http_util_object_fe[];

extern PHP_MINIT_FUNCTION(http_util_object);

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

