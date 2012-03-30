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

#ifndef PHP_HTTP_CURL_CLIENT_POOL_H
#define PHP_HTTP_CURL_CLIENT_POOL_H

#if PHP_HTTP_HAVE_CURL

PHP_HTTP_API php_http_client_pool_ops_t *php_http_curl_client_pool_get_ops(void);

extern zend_class_entry *php_http_curl_client_pool_class_entry;
extern zend_function_entry php_http_curl_client_pool_method_entry[];

extern zend_object_value php_http_curl_client_pool_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_curl_client_pool_object_new_ex(zend_class_entry *ce, php_http_client_pool_t *p, php_http_client_pool_object_t **ptr TSRMLS_DC);

#if PHP_HTTP_HAVE_EVENT
struct php_http_curl_globals {
	void *event_base;
};

PHP_RINIT_FUNCTION(http_curl_client_pool);
#endif

PHP_MINIT_FUNCTION(http_curl_client_pool);

#endif /* PHP_HTTP_HAVE_CURL */
#endif /* PHP_HTTP_CURL_CLIENT_POOL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

