/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: php_http_persistent_handle_api.h 292841 2009-12-31 08:48:57Z mike $ */

#ifndef PHP_HTTP_PERSISTENT_HANDLE_H
#define PHP_HTTP_PERSISTENT_HANDLE_H

typedef void *(*php_http_persistent_handle_ctor_t)(void);
typedef void (*php_http_persistent_handle_dtor_t)(void *handle);
typedef void *(*php_http_persistent_handle_copy_t)(void *handle);

struct php_http_persistent_handle_globals {
	ulong limit;
	struct {
		ulong h;
		char *s;
		size_t l;
	} ident;
};

PHP_MINIT_FUNCTION(http_persistent_handle);
PHP_MSHUTDOWN_FUNCTION(http_persistent_handle);

PHP_HTTP_API STATUS php_http_persistent_handle_provide(const char *name_str, size_t name_len, php_http_persistent_handle_ctor_t ctor, php_http_persistent_handle_dtor_t dtor, php_http_persistent_handle_copy_t copy);
PHP_HTTP_API void php_http_persistent_handle_cleanup(const char *name_str, size_t name_len, int current_ident_only TSRMLS_DC);
PHP_HTTP_API HashTable *php_http_persistent_handle_statall(HashTable *ht TSRMLS_DC);
PHP_HTTP_API STATUS php_http_persistent_handle_acquire(const char *name_str, size_t name_len, void **handle TSRMLS_DC);
PHP_HTTP_API STATUS php_http_persistent_handle_release(const char *name_str, size_t name_len, void **handle TSRMLS_DC);
PHP_HTTP_API STATUS php_http_persistent_handle_accrete(const char *name_str, size_t name_len, void *old_handle, void **new_handle TSRMLS_DC);

#endif /* PHP_HTTP_PERSISTENT_HANDLE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
