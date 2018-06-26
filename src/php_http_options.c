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

static void php_http_options_hash_dtor(zval *pData)
{
	php_http_option_t *opt = Z_PTR_P(pData);

	zval_internal_dtor(&opt->defval);
	zend_hash_destroy(&opt->suboptions.options);
	zend_string_release(opt->name);
	pefree(opt, opt->persistent);
}

php_http_options_t *php_http_options_init(php_http_options_t *registry, zend_bool persistent)
{
	if (!registry) {
		registry = pecalloc(1, sizeof(*registry), persistent);
	} else {
		memset(registry, 0, sizeof(*registry));
	}

	registry->persistent = persistent;
	zend_hash_init(&registry->options, 0, NULL, php_http_options_hash_dtor, persistent);

	return registry;
}

ZEND_RESULT_CODE php_http_options_apply(php_http_options_t *registry, HashTable *options, void *userdata)
{
	zval *entry, *val;
	php_http_option_t *opt;

	ZEND_HASH_FOREACH_VAL(&registry->options, entry)
	{
		opt = Z_PTR_P(entry);
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
	ZEND_HASH_FOREACH_END();

	return SUCCESS;
}

void php_http_options_dtor(php_http_options_t *registry)
{
	zend_hash_destroy(&registry->options);
}

void php_http_options_free(php_http_options_t **registry)
{
	if (*registry) {
		php_http_options_dtor(*registry);
		pefree(*registry, (*registry)->persistent);
		*registry = NULL;
	}
}

php_http_option_t *php_http_option_register(php_http_options_t *registry, const char *name_str, size_t name_len, ulong option, zend_uchar type)
{
	php_http_option_t opt;

	memset(&opt, 0, sizeof(opt));

	php_http_options_init(&opt.suboptions, registry->persistent);
	opt.suboptions.getter = registry->getter;
	opt.suboptions.setter = registry->setter;

	opt.persistent = registry->persistent;
	opt.name = zend_string_init(name_str, name_len, registry->persistent);
	opt.type = type;
	opt.option = option;

	switch ((opt.type = type)) {
	case IS_TRUE:
		ZVAL_TRUE(&opt.defval);
		break;

	case IS_FALSE:
		ZVAL_FALSE(&opt.defval);
		break;

	case IS_LONG:
		ZVAL_LONG(&opt.defval, 0);
		break;

	case IS_DOUBLE:
		ZVAL_DOUBLE(&opt.defval, 0);
		break;

	default:
		ZVAL_NULL(&opt.defval);
		break;
	}

	return zend_hash_update_mem(&registry->options, opt.name, &opt, sizeof(opt));
}

zval *php_http_option_get(php_http_option_t *opt, HashTable *options, void *userdata)
{
	if (options) {
		return zend_hash_find(options, opt->name);
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
