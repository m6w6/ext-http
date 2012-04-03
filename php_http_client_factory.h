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

#include "php_http_client.h"
#include "php_http_client_pool.h"
#include "php_http_client_datashare.h"

typedef struct php_http_client_factory_driver {
	php_http_client_ops_t *client_ops;
	php_http_client_pool_ops_t *client_pool_ops;
	php_http_client_datashare_ops_t *client_datashare_ops;
} php_http_client_factory_driver_t;

PHP_HTTP_API STATUS php_http_client_factory_add_driver(const char *name_str, size_t name_len, php_http_client_factory_driver_t *driver);
PHP_HTTP_API STATUS php_http_client_factory_get_driver(const char *name_str, size_t name_len, php_http_client_factory_driver_t *driver);

zend_class_entry *php_http_client_factory_get_class_entry(void);

#define php_http_client_factory_new php_http_object_new

PHP_METHOD(HttpClientFactory, __construct);
PHP_METHOD(HttpClientFactory, createClient);
PHP_METHOD(HttpClientFactory, createPool);
PHP_METHOD(HttpClientFactory, createDataShare);
PHP_METHOD(HttpClientFactory, getGlobalDataShareInstance);
PHP_METHOD(HttpClientFactory, getDriver);
PHP_METHOD(HttpClientFactory, getAvailableDrivers);

extern PHP_MINIT_FUNCTION(http_client_factory);
extern PHP_MSHUTDOWN_FUNCTION(http_client_factory);

#endif /* PHP_HTTP_REQUEST_FACTORY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

