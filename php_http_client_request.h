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

#ifndef PHP_HTTP_CLIENT_REQUEST_H
#define PHP_HTTP_CLIENT_REQUEST_H

extern zend_class_entry *php_http_client_request_class_entry;
extern zend_function_entry php_http_client_request_method_entry[];

PHP_METHOD(HttpClientRequest, __construct);
PHP_METHOD(HttpClientRequest, setContentType);
PHP_METHOD(HttpClientRequest, getContentType);
PHP_METHOD(HttpClientRequest, setQuery);
PHP_METHOD(HttpClientRequest, getQuery);
PHP_METHOD(HttpClientRequest, addQuery);

PHP_MINIT_FUNCTION(http_client_request);

#endif /* PHP_HTTP_CLIENT_REQUEST_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
