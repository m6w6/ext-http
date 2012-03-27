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

#ifndef PHP_HTTP_CLIENT_DATASHARE_H
#define PHP_HTTP_CLIENT_DATASHARE_H

typedef enum php_http_client_datashare_setopt_opt {
	PHP_HTTP_CLIENT_DATASHARE_OPT_COOKIES,
	PHP_HTTP_CLIENT_DATASHARE_OPT_RESOLVER,
	PHP_HTTP_CLIENT_DATASHARE_OPT_SSLSESSIONS,
} php_http_client_datashare_setopt_opt_t;

typedef struct php_http_client_datashare *(*php_http_client_datashare_init_func_t)(struct php_http_client_datashare *h, void *init_arg);
typedef struct php_http_client_datashare *(*php_http_client_datashare_copy_func_t)(struct php_http_client_datashare *from, struct php_http_client_datashare *to);
typedef void (*php_http_client_datashare_dtor_func_t)(struct php_http_client_datashare *h);
typedef void (*php_http_client_datashare_reset_func_t)(struct php_http_client_datashare *h);
typedef STATUS (*php_http_client_datashare_attach_func_t)(struct php_http_client_datashare *h, php_http_client_t *client);
typedef STATUS (*php_http_client_datashare_detach_func_t)(struct php_http_client_datashare *h, php_http_client_t *client);
typedef STATUS (*php_http_client_datashare_setopt_func_t)(struct php_http_client_datashare *h, php_http_client_datashare_setopt_opt_t opt, void *arg);

typedef struct php_http_client_datashare_ops {
	php_http_resource_factory_ops_t *rsrc;
	php_http_client_datashare_init_func_t init;
	php_http_client_datashare_copy_func_t copy;
	php_http_client_datashare_dtor_func_t dtor;
	php_http_client_datashare_reset_func_t reset;
	php_http_client_datashare_attach_func_t attach;
	php_http_client_datashare_detach_func_t detach;
	php_http_client_datashare_setopt_func_t setopt;
	php_http_new_t create_object;
	zend_class_entry *(*class_entry)(void);
} php_http_client_datashare_ops_t;

typedef struct php_http_client_datashare {
	void *ctx;
	php_http_resource_factory_t *rf;
	php_http_client_datashare_ops_t *ops;
	zend_llist clients;
#ifdef ZTS
	void ***ts;
#endif
} php_http_client_datashare_t;

extern PHP_MINIT_FUNCTION(http_client_datashare);

PHP_HTTP_API php_http_client_datashare_t *php_http_client_datashare_init(php_http_client_datashare_t *h, php_http_client_datashare_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC);
PHP_HTTP_API php_http_client_datashare_t *php_http_client_datashare_copy(php_http_client_datashare_t *from, php_http_client_datashare_t *to);
PHP_HTTP_API void php_http_client_datashare_dtor(php_http_client_datashare_t *h);
PHP_HTTP_API void php_http_client_datashare_free(php_http_client_datashare_t **h);
PHP_HTTP_API STATUS php_http_client_datashare_attach(php_http_client_datashare_t *h, zval *client);
PHP_HTTP_API STATUS php_http_client_datashare_detach(php_http_client_datashare_t *h, zval *client);
PHP_HTTP_API STATUS php_http_client_datashare_setopt(php_http_client_datashare_t *h, php_http_client_datashare_setopt_opt_t opt, void *arg);
PHP_HTTP_API void php_http_client_datashare_reset(php_http_client_datashare_t *h);

typedef struct php_http_client_datashare_object {
	zend_object zo;
	php_http_client_datashare_t *share;
} php_http_client_datashare_object_t;

extern zend_class_entry *php_http_client_datashare_class_entry;
extern zend_function_entry php_http_client_datashare_method_entry[];

extern zend_object_value php_http_client_datashare_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_client_datashare_object_new_ex(zend_class_entry *ce, php_http_client_datashare_t *share, php_http_client_datashare_object_t **ptr TSRMLS_DC);
extern void php_http_client_datashare_object_free(void *object TSRMLS_DC);

extern zend_object_handlers *php_http_client_datashare_get_object_handlers(void);

PHP_METHOD(HttpClientDataShare, __destruct);
PHP_METHOD(HttpClientDataShare, count);
PHP_METHOD(HttpClientDataShare, attach);
PHP_METHOD(HttpClientDataShare, detach);
PHP_METHOD(HttpClientDataShare, reset);

#endif /* PHP_HTTP_CLIENT_DATASHARE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

