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

#ifndef PHP_HTTP_PARAMS_H
#define PHP_HTTP_PARAMS_H

typedef struct php_http_params_token {
	char *str;
	size_t len;
} php_http_params_token_t;

#define PHP_HTTP_PARAMS_RAW			0x00
#define PHP_HTTP_PARAMS_ESCAPED		0x01
#define PHP_HTTP_PARAMS_URLENCODED	0x04
#define PHP_HTTP_PARAMS_DIMENSION	0x08
#define PHP_HTTP_PARAMS_RFC5987		0x10
#define PHP_HTTP_PARAMS_RFC5988		0x20
#define PHP_HTTP_PARAMS_QUERY		(PHP_HTTP_PARAMS_URLENCODED|PHP_HTTP_PARAMS_DIMENSION)
#define PHP_HTTP_PARAMS_DEFAULT		(PHP_HTTP_PARAMS_ESCAPED|PHP_HTTP_PARAMS_RFC5987)

typedef struct php_http_params_opts {
	php_http_params_token_t input;
	php_http_params_token_t **param;
	php_http_params_token_t **arg;
	php_http_params_token_t **val;
	zval defval;
	unsigned flags;
} php_http_params_opts_t;

PHP_HTTP_API php_http_params_opts_t *php_http_params_opts_default_get(php_http_params_opts_t *opts);
PHP_HTTP_API HashTable *php_http_params_parse(HashTable *params, const php_http_params_opts_t *opts);
PHP_HTTP_API php_http_buffer_t *php_http_params_to_string(php_http_buffer_t *buf, HashTable *params, const char *pss, size_t psl, const char *ass, size_t asl, const char *vss, size_t vsl, unsigned flags);

PHP_HTTP_API php_http_params_token_t **php_http_params_separator_init(zval *zv);
PHP_HTTP_API void php_http_params_separator_free(php_http_params_token_t **separator);

typedef php_http_object_t php_http_params_object_t;

PHP_HTTP_API zend_class_entry *php_http_params_get_class_entry(void);

PHP_MINIT_FUNCTION(http_params);

#define php_http_params_object_new php_http_object_new
#define php_http_params_object_new_ex php_http_object_new_ex

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

