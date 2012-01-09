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

PHP_HTTP_API php_http_resource_factory_t *php_http_resource_factory_init(php_http_resource_factory_t *f, php_http_resource_factory_ops_t *fops, void *data, void (*dtor)(void *data))
{
	if (!f) {
		f = emalloc(sizeof(*f));
	}
	memset(f, 0, sizeof(*f));

	memcpy(&f->fops, fops, sizeof(*fops));

	f->data = data;
	f->dtor = dtor;

	return f;
}

PHP_HTTP_API void php_http_resource_factory_dtor(php_http_resource_factory_t *f)
{
	if (f->dtor) {
		f->dtor(f->data);
	}
}
PHP_HTTP_API void php_http_resource_factory_free(php_http_resource_factory_t **f)
{
	if (*f) {
		php_http_resource_factory_dtor(*f);
		efree(*f);
		*f = NULL;
	}
}

PHP_HTTP_API void *php_http_resource_factory_handle_ctor(php_http_resource_factory_t *f TSRMLS_DC)
{
	if (f->fops.ctor) {
		return f->fops.ctor(f->data TSRMLS_CC);
	}
	return NULL;
}

PHP_HTTP_API void *php_http_resource_factory_handle_copy(php_http_resource_factory_t *f, void *handle TSRMLS_DC)
{
	if (f->fops.copy) {
		return f->fops.copy(f->data, handle TSRMLS_CC);
	}
	return NULL;
}

PHP_HTTP_API void php_http_resource_factory_handle_dtor(php_http_resource_factory_t *f, void *handle TSRMLS_DC)
{
	if (f->fops.dtor) {
		f->fops.dtor(f->data, handle TSRMLS_CC);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

