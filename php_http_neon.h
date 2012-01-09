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

#ifndef PHP_HTTP_NEON_H
#define PHP_HTTP_NEON_H

#if PHP_HTTP_HAVE_NEON

#include "php_http_request.h"

php_http_request_ops_t *php_http_neon_get_request_ops(void);

PHP_MINIT_FUNCTION(http_neon);
PHP_MSHUTDOWN_FUNCTION(http_neon);

extern zend_class_entry *php_http_neon_class_entry;
extern zend_function_entry php_http_neon_method_entry[];

#define php_http_neon_new php_http_object_new

PHP_METHOD(HttpNEON, __construct);

#endif /* PHP_HTTP_HAVE_NEON */
#endif /* PHP_HTTP_NEON_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

