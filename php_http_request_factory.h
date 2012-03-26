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

#ifndef PHP_HTTP_REQUEST_FACTORY_H
#define PHP_HTTP_REQUEST_FACTORY_H

#include "php_http_request.h"
#include "php_http_request_pool.h"
#include "php_http_request_datashare.h"

typedef struct php_http_request_factory_driver {
	php_http_client_ops_t *request_ops;
	php_http_request_pool_ops_t *request_pool_ops;
	php_http_request_datashare_ops_t *request_datashare_ops;
} php_http_request_factory_driver_t;

PHP_HTTP_API STATUS php_http_request_factory_add_driver(const char *name_str, size_t name_len, php_http_request_factory_driver_t *driver);
PHP_HTTP_API STATUS php_http_request_factory_get_driver(const char *name_str, size_t name_len, php_http_request_factory_driver_t *driver);

extern zend_class_entry *php_http_request_factory_class_entry;
extern zend_function_entry php_http_request_factory_method_entry[];

#define php_http_request_factory_new php_http_object_new

PHP_METHOD(HttpRequestFactory, __construct);
PHP_METHOD(HttpRequestFactory, createRequest);
PHP_METHOD(HttpRequestFactory, createPool);
PHP_METHOD(HttpRequestFactory, createDataShare);
PHP_METHOD(HttpRequestFactory, getGlobalDataShareInstance);
PHP_METHOD(HttpRequestFactory, getDriver);
PHP_METHOD(HttpRequestFactory, getAvailableDrivers);

extern PHP_MINIT_FUNCTION(http_request_factory);
extern PHP_MSHUTDOWN_FUNCTION(http_request_factory);

#endif /* PHP_HTTP_REQUEST_FACTORY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

