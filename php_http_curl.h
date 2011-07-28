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

#ifndef PHP_HTTP_CURL_H
#define PHP_HTTP_CURL_H

#if PHP_HTTP_HAVE_CURL

PHP_HTTP_API php_http_request_ops_t *php_http_curl_get_request_ops(void);
PHP_HTTP_API php_http_request_pool_ops_t *php_http_curl_get_request_pool_ops(void);
PHP_HTTP_API php_http_request_datashare_ops_t *php_http_curl_get_request_datashare_ops(void);

extern PHP_MINIT_FUNCTION(http_curl);
extern PHP_MSHUTDOWN_FUNCTION(http_curl);
extern PHP_RINIT_FUNCTION(http_curl);

#if PHP_HTTP_HAVE_EVENT
struct php_http_curl_globals {
	void *event_base;
};
#endif

extern zend_class_entry *php_http_curl_class_entry;
extern zend_function_entry php_http_curl_method_entry[];

#define php_http_curl_new php_http_object_new

PHP_METHOD(HttpCURL, __construct);

#endif /* PHP_HTTP_HAVE_CURL */
#endif /* PHP_HTTP_CURL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

