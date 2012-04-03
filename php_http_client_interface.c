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

#include "php_http_api.h"
#include "php_http_client.h"

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpClient, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpClient, method, 0)
#define PHP_HTTP_CLIENT_ME(method)	PHP_ABSTRACT_ME(HttpClient, method, PHP_HTTP_ARGS(HttpClient, method))

PHP_HTTP_BEGIN_ARGS(send, 1)
	PHP_HTTP_ARG_VAL(request, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_client_interface_class_entry;

zend_class_entry *php_http_client_interface_get_class_entry(void)
{
	return php_http_client_interface_class_entry;
}

zend_function_entry php_http_client_interface_method_entry[] = {
	PHP_HTTP_CLIENT_ME(send)
	{NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(http_client_interface)
{
	PHP_HTTP_REGISTER_INTERFACE(http, Client, http_client_interface, ZEND_ACC_INTERFACE);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

