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

#ifndef PHP_HTTP_ENV_REQUEST_H
#define PHP_HTTP_ENV_REQUEST_H

zend_class_entry *php_http_env_request_get_class_entry(void);

PHP_METHOD(HttpEnvRequest, __construct);
PHP_METHOD(HttpEnvRequest, getForm);
PHP_METHOD(HttpEnvRequest, getQuery);
PHP_METHOD(HttpEnvRequest, getFiles);

PHP_MINIT_FUNCTION(http_env_request);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
