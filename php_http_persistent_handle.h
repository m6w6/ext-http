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

#ifndef PHP_HTTP_PERSISTENT_HANDLE_H
#define PHP_HTTP_PERSISTENT_HANDLE_H

typedef struct php_http_persistent_handle_factory {
	void *provider;

	struct {
		char *str;
		size_t len;
	} ident;

	unsigned free_on_abandon:1;
} php_http_persistent_handle_factory_t;

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

PHP_HTTP_API STATUS php_http_persistent_handle_provide(const char *name_str, size_t name_len, php_http_resource_factory_ops_t *fops, void *data, void (*dtor)(void *));
PHP_HTTP_API php_http_persistent_handle_factory_t *php_http_persistent_handle_concede(php_http_persistent_handle_factory_t *a, const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC);
PHP_HTTP_API void php_http_persistent_handle_abandon(php_http_persistent_handle_factory_t *a);
PHP_HTTP_API void *php_http_persistent_handle_acquire(php_http_persistent_handle_factory_t *a TSRMLS_DC);
PHP_HTTP_API void php_http_persistent_handle_release(php_http_persistent_handle_factory_t *a, void *handle TSRMLS_DC);
PHP_HTTP_API void *php_http_persistent_handle_accrete(php_http_persistent_handle_factory_t *a, void *handle TSRMLS_DC);

PHP_HTTP_API php_http_resource_factory_ops_t *php_http_persistent_handle_resource_factory_ops(void);

PHP_HTTP_API void php_http_persistent_handle_cleanup(const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC);
PHP_HTTP_API HashTable *php_http_persistent_handle_statall(HashTable *ht TSRMLS_DC);

#endif /* PHP_HTTP_PERSISTENT_HANDLE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
