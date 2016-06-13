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

#ifndef PHP_HTTP_DEBUG_NEG
# define PHP_HTTP_DEBUG_NEG 0
#endif

static int php_http_negotiate_sort(const void *first, const void *second)
{
	Bucket *b1 = (Bucket *) first, *b2 = (Bucket *) second;
	int result = numeric_compare_function(&b1->val, &b2->val);

	return (result > 0 ? -1 : (result < 0 ? 1 : 0));
}

#define M_PRI 5
#define M_SEC 2
#define M_ANY -1
#define M_NOT 0
#define M_ALL ~0
static inline unsigned php_http_negotiate_match(const char *param_str, size_t param_len, const char *supported_str, size_t supported_len, const char *sep_str, size_t sep_len)
{
	unsigned match = M_NOT;

	if (param_len == supported_len && !strncasecmp(param_str, supported_str, param_len)) {
		/* that was easy */
		match = M_ALL;
	} else if (sep_str && sep_len) {
		const char *param_sec = php_http_locate_str(param_str, param_len, sep_str, sep_len);
		size_t param_pri_len = param_sec ? param_sec - param_str : param_len;
		const char *supported_sec = php_http_locate_str(supported_str, supported_len, sep_str, sep_len);
		size_t supported_pri_len = supported_sec ? supported_sec - supported_str : supported_len;
		size_t cmp_len = MIN(param_pri_len, supported_pri_len);

		if (((*param_str == '*') || (*supported_str == '*'))
		||	((param_pri_len == supported_pri_len) && !strncasecmp(param_str, supported_str, param_pri_len))
		||	((!param_sec || !supported_sec) && cmp_len && !strncasecmp(param_str, supported_str, cmp_len))
		) {
			match += M_PRI;
		}

		if (param_sec && supported_sec
		&& ((*(param_sec + sep_len) == '*' || *(supported_sec + sep_len) == '*')
			|| !strcasecmp(param_sec, supported_sec)
			)
		) {
			match += M_SEC;
		}

		if ((param_sec && *(param_sec + sep_len) == '*')
		||	(supported_sec && *(supported_sec + sep_len) == '*')
		||	((*param_str == '*') || (*supported_str == '*'))
		) {
			match += M_ANY;
		}
	}
#if PHP_HTTP_DEBUG_NEG
	fprintf(stderr, "match: %s == %s => %u\n", supported_str, param_str, match);
#endif
	return match;
}
static int php_http_negotiate_reduce(zval *p, int num_args, va_list args, zend_hash_key *hash_key)
{
	unsigned best_match = 0;
	double q = 0;
	php_http_arrkey_t key;
	zval *value;
	zend_string *supported = zval_get_string(p);
	HashTable *params = va_arg(args, HashTable *);
	HashTable *result = va_arg(args, HashTable *);
	const char *sep_str = va_arg(args, const char *);
	size_t sep_len = va_arg(args, size_t);

	ZEND_HASH_FOREACH_KEY_VAL(params, key.h, key.key, value)
	{
		unsigned match;

#if PHP_HTTP_DEBUG_NEG
			fprintf(stderr, "match(%u) > best_match(%u) = %u (q=%f)\n", match, best_match, match>best_match, Z_DVAL_PP(val));
#endif
		php_http_arrkey_stringify(&key, NULL);
		match = php_http_negotiate_match(key.key->val, key.key->len, supported->val, supported->len, sep_str, sep_len);

		if (match > best_match) {
			best_match = match;
			q = Z_DVAL_P(value) - 0.1 / match;
		}
		php_http_arrkey_dtor(&key);
	}
	ZEND_HASH_FOREACH_END();

	if (q > 0) {
		zval tmp;

		ZVAL_DOUBLE(&tmp, q);
		zend_hash_update(result, supported, &tmp);
	}

	zend_string_release(supported);
	return ZEND_HASH_APPLY_KEEP;
}

HashTable *php_http_negotiate(const char *value_str, size_t value_len, HashTable *supported, const char *primary_sep_str, size_t primary_sep_len)
{
	HashTable *result = NULL;

	if (value_str && value_len) {
		unsigned i = 0;
		zval arr, *val, *arg, *zq;
		HashTable params;
		php_http_arrkey_t key;
		php_http_params_opts_t opts;

		zend_hash_init(&params, 10, NULL, ZVAL_PTR_DTOR, 0);
		php_http_params_opts_default_get(&opts);
		opts.input.str = estrndup(value_str, value_len);
		opts.input.len = value_len;
		opts.flags &= ~PHP_HTTP_PARAMS_RFC5987;
		php_http_params_parse(&params, &opts);
		efree(opts.input.str);

		array_init(&arr);

		ZEND_HASH_FOREACH_KEY_VAL(&params, key.h, key.key, val)
		{
			double q;

			if ((arg = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("arguments")))
			&&	(IS_ARRAY == Z_TYPE_P(arg))
			&&	(zq = zend_hash_str_find(Z_ARRVAL_P(arg), ZEND_STRL("q")))) {
				q = zval_get_double(zq);
			} else {
				q = 1.0 - (((double) ++i) / 100.0);
			}

#if 0
			fprintf(stderr, "Q: %s=%1.3f\n", key.key->val, q);
#endif

			if (key.key) {
				add_assoc_double_ex(&arr, key.key->val, key.key->len, q);
			} else {
				add_index_double(&arr, key.h, q);
			}

		}
		ZEND_HASH_FOREACH_END();

#if PHP_HTTP_DEBUG_NEG
		zend_print_zval_r(&arr, 1);
#endif

		ALLOC_HASHTABLE(result);
		zend_hash_init(result, zend_hash_num_elements(supported), NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_apply_with_arguments(supported, php_http_negotiate_reduce, 4, Z_ARRVAL(arr), result, primary_sep_str, primary_sep_len);
		zend_hash_destroy(&params);
		zval_dtor(&arr);
		zend_hash_sort(result, php_http_negotiate_sort, 0);
	}

	return result;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */


