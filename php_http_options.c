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

PHP_HTTP_API php_http_options_t *php_http_options_init(php_http_options_t *registry, zend_bool persistent)
{
	if (!registry) {
		registry = pecalloc(1, sizeof(*registry), persistent);
	} else {
		memset(registry, 0, sizeof(*registry));
	}

	registry->persistent = persistent;
	zend_hash_init(&registry->options, 0, NULL, (dtor_func_t) zend_hash_destroy, persistent);

	return registry;
}

PHP_HTTP_API STATUS php_http_options_apply(php_http_options_t *registry, HashTable *options, void *userdata)
{
	HashPosition pos;
	zval *val;
	php_http_option_t *opt;

	FOREACH_HASH_VAL(pos, &registry->options, opt) {
		if (!(val = registry->getter(opt, options, userdata))) {
			val = &opt->defval;
		}
		if (registry->setter) {
			if (SUCCESS != registry->setter(opt, val, userdata)) {
				return FAILURE;
			}
		} else if (!opt->setter || SUCCESS != opt->setter(opt, val, userdata)) {
			return FAILURE;
		}
	}
	return SUCCESS;
}

PHP_HTTP_API void php_http_options_dtor(php_http_options_t *registry)
{
	zend_hash_destroy(&registry->options);
}

PHP_HTTP_API void php_http_options_free(php_http_options_t **registry)
{
	if (*registry) {
		php_http_options_dtor(*registry);
		pefree(*registry, (*registry)->persistent);
		*registry = NULL;
	}
}

PHP_HTTP_API php_http_option_t *php_http_option_register(php_http_options_t *registry, const char *name_str, size_t name_len, ulong option, zend_uchar type)
{
	php_http_option_t opt, *dst = NULL;

	memset(&opt, 0, sizeof(opt));

	php_http_options_init(&opt.suboptions, registry->persistent);
	opt.suboptions.getter = registry->getter;
	opt.suboptions.setter = registry->setter;

	opt.name.h = zend_hash_func(opt.name.s = name_str, opt.name.l = name_len + 1);
	opt.type = type;
	opt.option = option;

	INIT_ZVAL(opt.defval);
	switch ((opt.type = type)) {
	case IS_BOOL:
		ZVAL_BOOL(&opt.defval, 0);
		break;

	case IS_LONG:
		ZVAL_LONG(&opt.defval, 0);
		break;

	case IS_STRING:
		ZVAL_STRINGL(&opt.defval, NULL, 0, 0);
		break;

	case IS_DOUBLE:
		ZVAL_DOUBLE(&opt.defval, 0);
		break;

	default:
		ZVAL_NULL(&opt.defval);
		break;
	}

	zend_hash_quick_update(&registry->options, opt.name.s, opt.name.l, opt.name.h, (void *) &opt, sizeof(opt), (void *) &dst);
	return dst;
}

PHP_HTTP_API zval *php_http_option_get(php_http_option_t *opt, HashTable *options, void *userdata)
{
	if (options) {
		zval **zoption;

		if (SUCCESS == zend_hash_quick_find(options, opt->name.s, opt->name.l, opt->name.h, (void *) &zoption)) {
			return *zoption;
		}
	}

	return NULL;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
