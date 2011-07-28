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

#ifndef PHP_HTTP_PARAMS_H
#define PHP_HTTP_PARAMS_H

#define PHP_HTTP_PARAMS_ALLOW_COMMA			0x01
#define PHP_HTTP_PARAMS_ALLOW_FAILURE		0x02
#define PHP_HTTP_PARAMS_RAISE_ERROR			0x04
#define PHP_HTTP_PARAMS_DEFAULT	(PHP_HTTP_PARAMS_ALLOW_COMMA|PHP_HTTP_PARAMS_ALLOW_FAILURE|PHP_HTTP_PARAMS_RAISE_ERROR)
#define PHP_HTTP_PARAMS_COLON_SEPARATOR		0x10

typedef void (*php_http_params_parse_func_t)(void *cb_arg, const char *key, int keylen, const char *val, int vallen TSRMLS_DC);

PHP_HTTP_API void php_http_params_parse_default_func(void *ht, const char *key, int keylen, const char *val, int vallen TSRMLS_DC);
PHP_HTTP_API STATUS php_http_params_parse(const char *params, int flags, php_http_params_parse_func_t cb, void *cb_arg TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

