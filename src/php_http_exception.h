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

#ifndef PHP_HTTP_EXCEPTION_H
#define PHP_HTTP_EXCEPTION_H

/* short hand for zend_throw_exception_ex */
#define php_http_throw(e, fmt, ...) \
	zend_throw_exception_ex(php_http_get_exception_ ##e## _class_entry(), 0, fmt, __VA_ARGS__)

/* wrap a call with replaced zend_error_handling */
#define php_http_expect(test, e, fail) \
	do { \
		zend_error_handling __zeh; \
		zend_replace_error_handling(EH_THROW, php_http_get_exception_ ##e## _class_entry(), &__zeh); \
		if (UNEXPECTED(!(test))) { \
			zend_restore_error_handling(&__zeh); \
			fail; \
		} \
		zend_restore_error_handling(&__zeh); \
	} while(0)

PHP_HTTP_API zend_class_entry *php_http_get_exception_interface_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_runtime_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_unexpected_val_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_method_call_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_invalid_arg_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_header_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_url_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_message_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_conversion_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_exception_bad_querystring_class_entry(void);

PHP_MINIT_FUNCTION(http_exception);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

