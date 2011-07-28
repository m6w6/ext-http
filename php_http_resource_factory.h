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

#ifndef PHP_HTTP_RESOURCE_FACTORY_H
#define PHP_HTTP_RESOURCE_FACTORY_H

typedef void *(*php_http_resource_factory_handle_ctor_t)(void *opaque TSRMLS_DC);
typedef void *(*php_http_resource_factory_handle_copy_t)(void *opaque, void *handle TSRMLS_DC);
typedef void (*php_http_resource_factory_handle_dtor_t)(void *opaque, void *handle TSRMLS_DC);

typedef struct php_http_resource_factory_ops {
	php_http_resource_factory_handle_ctor_t ctor;
	php_http_resource_factory_handle_copy_t copy;
	php_http_resource_factory_handle_dtor_t dtor;
} php_http_resource_factory_ops_t;

typedef struct php_http_resource_factory {
	php_http_resource_factory_ops_t fops;

	void *data;
	void (*dtor)(void *data);

} php_http_resource_factory_t;

PHP_HTTP_API php_http_resource_factory_t *php_http_resource_factory_init(php_http_resource_factory_t *f, php_http_resource_factory_ops_t *fops, void *data, void (*dtor)(void *data));
PHP_HTTP_API void php_http_resource_factory_dtor(php_http_resource_factory_t *f);
PHP_HTTP_API void php_http_resource_factory_free(php_http_resource_factory_t **f);

PHP_HTTP_API void *php_http_resource_factory_handle_ctor(php_http_resource_factory_t *f TSRMLS_DC);
PHP_HTTP_API void *php_http_resource_factory_handle_copy(php_http_resource_factory_t *f, void *handle TSRMLS_DC);
PHP_HTTP_API void php_http_resource_factory_handle_dtor(php_http_resource_factory_t *f, void *handle TSRMLS_DC);

#endif /* PHP_HTTP_RESOURCE_FACTORY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
