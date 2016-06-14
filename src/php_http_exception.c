/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

#include "ext/spl/spl_exceptions.h"

#ifndef PHP_HTTP_DBG_EXCEPTIONS
#	define PHP_HTTP_DBG_EXCEPTIONS 0
#endif

#if PHP_HTTP_DBG_EXCEPTIONS
static void php_http_exception_hook(zval *ex)
{
	if (ex) {
		zval *m = zend_read_property(Z_OBJCE_P(ex), ex, "message", lenof("message"), 0);
		fprintf(stderr, "*** Threw exception '%s'\n", Z_STRVAL_P(m));
	} else {
		fprintf(stderr, "*** Threw NULL exception\n");
	}
}
#endif

static zend_class_entry *php_http_exception_interface_class_entry;
zend_class_entry *php_http_get_exception_interface_class_entry(void)
{
	return php_http_exception_interface_class_entry;
}
static zend_class_entry *php_http_exception_runtime_class_entry;
zend_class_entry *php_http_get_exception_runtime_class_entry(void)
{
	return php_http_exception_runtime_class_entry;
}
static zend_class_entry *php_http_exception_unexpected_val_class_entry;
zend_class_entry *php_http_get_exception_unexpected_val_class_entry(void)
{
	return php_http_exception_unexpected_val_class_entry;
}
static zend_class_entry *php_http_exception_bad_method_call_class_entry;
zend_class_entry *php_http_get_exception_bad_method_call_class_entry(void)
{
	return php_http_exception_bad_method_call_class_entry;
}
static zend_class_entry *php_http_exception_invalid_arg_class_entry;
zend_class_entry *php_http_get_exception_invalid_arg_class_entry(void)
{
	return php_http_exception_invalid_arg_class_entry;
}
static zend_class_entry *php_http_exception_bad_header_class_entry;
zend_class_entry *php_http_get_exception_bad_header_class_entry(void)
{
	return php_http_exception_bad_header_class_entry;
}
static zend_class_entry *php_http_exception_bad_url_class_entry;
zend_class_entry *php_http_get_exception_bad_url_class_entry(void)
{
	return php_http_exception_bad_url_class_entry;
}
static zend_class_entry *php_http_exception_bad_message_class_entry;
zend_class_entry *php_http_get_exception_bad_message_class_entry(void)
{
	return php_http_exception_bad_message_class_entry;
}
static zend_class_entry *php_http_exception_bad_conversion_class_entry;
zend_class_entry *php_http_get_exception_bad_conversion_class_entry(void)
{
	return php_http_exception_bad_conversion_class_entry;
}
static zend_class_entry *php_http_exception_bad_querystring_class_entry;
zend_class_entry *php_http_get_exception_bad_querystring_class_entry(void)
{
	return php_http_exception_bad_querystring_class_entry;
}

PHP_MINIT_FUNCTION(http_exception)
{
	zend_class_entry *cep, ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Exception", NULL);
	php_http_exception_interface_class_entry = zend_register_internal_interface(&ce);
	
	/*
	 * Would be great to only have a few exceptions and rather more identifying
	 * error codes, but zend_replace_error_handling() does not accept any codes.
	 */

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "RuntimeException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_RuntimeException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_runtime_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "UnexpectedValueException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_UnexpectedValueException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_unexpected_val_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadMethodCallException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_BadMethodCallException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_method_call_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "InvalidArgumentException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_InvalidArgumentException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_invalid_arg_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadHeaderException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_DomainException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_header_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadUrlException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_DomainException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_url_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadMessageException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_DomainException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_message_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadConversionException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_DomainException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_conversion_class_entry = cep;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Exception", "BadQueryStringException", NULL);
	cep = zend_register_internal_class_ex(&ce, spl_ce_DomainException);
	zend_class_implements(cep, 1, php_http_exception_interface_class_entry);
	php_http_exception_bad_querystring_class_entry = cep;

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

