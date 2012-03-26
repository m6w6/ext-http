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

#ifndef PHP_HTTP_CLIENT_DATASHARE_CURL_H
#define PHP_HTTP_CLIENT_DATASHARE_CURL_H

#if PHP_HTTP_HAVE_CURL

PHP_HTTP_API php_http_client_datashare_ops_t *php_http_client_datashare_curl_get_ops(void);

extern zend_object_value php_http_client_datashare_curl_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_client_datashare_curl_object_new_ex(zend_class_entry *ce, php_http_client_datashare_t *share, php_http_client_datashare_object_t **ptr TSRMLS_DC);

PHP_MINIT_FUNCTION(http_client_datashare_curl);

#endif /* PHP_HTTP_HAVE_CURL */

#endif /* PHP_HTTP_CLIENT_DATASHARE_CURL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

