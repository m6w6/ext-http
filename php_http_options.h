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

#ifndef PHP_HTTP_OPTIONS_H
#define PHP_HTTP_OPTIONS_H

typedef struct php_http_option php_http_option_t;
typedef struct php_http_options php_http_options_t;

typedef STATUS (*php_http_option_set_callback_t)(php_http_option_t *opt, zval *val, void *userdata);
typedef zval *(*php_http_option_get_callback_t)(php_http_option_t *opt, HashTable *options, void *userdata);

struct php_http_options {
	HashTable options;

	php_http_option_get_callback_t getter;
	php_http_option_set_callback_t setter;

	unsigned persistent:1;
};

struct php_http_option {
	php_http_options_t suboptions;

	struct {
		const char *s;
		size_t l;
		ulong h;
	} name;

	ulong option;
	zend_uchar type;
	unsigned flags;
	zval defval;

	php_http_option_set_callback_t setter;
};

PHP_HTTP_API php_http_options_t *php_http_options_init(php_http_options_t *registry, zend_bool persistent);
PHP_HTTP_API STATUS php_http_options_apply(php_http_options_t *registry, HashTable *options, void *userdata);
PHP_HTTP_API void php_http_options_dtor(php_http_options_t *registry);
PHP_HTTP_API void php_http_options_free(php_http_options_t **registry);

PHP_HTTP_API php_http_option_t *php_http_option_register(php_http_options_t *registry, const char *name_str, size_t name_len, ulong option, zend_uchar type);
PHP_HTTP_API zval *php_http_option_get(php_http_option_t *opt, HashTable *options, void *userdata);

#endif /* PHP_HTTP_OPTIONS_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
