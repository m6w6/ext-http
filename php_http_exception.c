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

#include <zend_exceptions.h>

#ifndef PHP_HTTP_DBG_EXCEPTIONS
#	define PHP_HTTP_DBG_EXCEPTIONS 0
#endif

static zend_class_entry *php_http_exception_class_entry;

zend_class_entry *php_http_exception_get_class_entry(void)
{
	return php_http_exception_class_entry;
}

#if PHP_HTTP_DBG_EXCEPTIONS
static void php_http_exception_hook(zval *ex TSRMLS_DC)
{
	if (ex) {
		zval *m = zend_read_property(Z_OBJCE_P(ex), ex, "message", lenof("message"), 0 TSRMLS_CC);
		fprintf(stderr, "*** Threw exception '%s'\n", Z_STRVAL_P(m));
	} else {
		fprintf(stderr, "*** Threw NULL exception\n");
	}
}
#endif

PHP_MINIT_FUNCTION(http_exception)
{
	PHP_HTTP_REGISTER_EXCEPTION(Exception, php_http_exception_class_entry, zend_exception_get_default(TSRMLS_C));
	
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_UNKNOWN"), PHP_HTTP_E_UNKNOWN TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_RUNTIME"), PHP_HTTP_E_RUNTIME TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_INVALID_PARAM"), PHP_HTTP_E_INVALID_PARAM TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_HEADER"), PHP_HTTP_E_HEADER TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_MALFORMED_HEADERS"), PHP_HTTP_E_MALFORMED_HEADERS TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_MESSAGE"), PHP_HTTP_E_MESSAGE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_MESSAGE_TYPE"), PHP_HTTP_E_MESSAGE_TYPE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_MESSAGE_BODY"), PHP_HTTP_E_MESSAGE_BODY TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_ENCODING"), PHP_HTTP_E_ENCODING TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_CLIENT"), PHP_HTTP_E_CLIENT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_CLIENT_POOL"), PHP_HTTP_E_CLIENT_POOL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_CLIENT_DATASHARE"), PHP_HTTP_E_CLIENT_DATASHARE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_SOCKET"), PHP_HTTP_E_SOCKET TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_RESPONSE"), PHP_HTTP_E_RESPONSE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_URL"), PHP_HTTP_E_URL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_QUERYSTRING"), PHP_HTTP_E_QUERYSTRING TSRMLS_CC);
	zend_declare_class_constant_long(php_http_exception_get_class_entry(), ZEND_STRL("E_COOKIE"), PHP_HTTP_E_COOKIE TSRMLS_CC);
	
#if PHP_HTTP_DBG_EXCEPTIONS
	zend_throw_exception_hook = php_http_exception_hook;
#endif
	
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

