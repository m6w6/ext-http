/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

static int php_http_negotiate_sort(const void *a, const void *b TSRMLS_DC)
{
	zval result, *first, *second;

	first = *((zval **) (*((Bucket **) a))->pData);
	second= *((zval **) (*((Bucket **) b))->pData);

	if (numeric_compare_function(&result, first, second TSRMLS_CC) != SUCCESS) {
		return 0;
	}
	return (Z_LVAL(result) > 0 ? -1 : (Z_LVAL(result) < 0 ? 1 : 0));
}

#define M_PRI 5
#define M_SEC 2
#define M_ANY 1
#define M_NOT 0
#define M_ALL -1
static inline unsigned php_http_negotiate_match(const char *param_str, size_t param_len, const char *supported_str, size_t supported_len, const char *sep_str, size_t sep_len)
{
	int match = M_NOT;

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

		if (param_sec && supported_sec && !strcasecmp(param_sec, supported_sec)) {
			match += M_SEC;
		}

		if ((param_sec && *(param_sec + sep_len) == '*')
		||	(supported_sec && *(supported_sec + sep_len) == '*')
		||	((*param_str == '*') || (*supported_str == '*'))
		) {
			match += M_ANY;
		}
	}
#if 0
	fprintf(stderr, "match: %s == %s => %u\n", supported_str, param_str, match);
#endif
	return match;
}

static int php_http_negotiate_reduce(void *p TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key)
{
	unsigned best_match = 0;
	HashPosition pos;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **q = NULL, **val, *supported = php_http_ztyp(IS_STRING, *(zval **)p);
	HashTable *params = va_arg(args, HashTable *);
	HashTable *result = va_arg(args, HashTable *);
	const char *sep_str = va_arg(args, const char *);
	size_t sep_len = va_arg(args, size_t);

	FOREACH_HASH_KEYVAL(pos, params, key, val) {
		if (key.type == HASH_KEY_IS_STRING) {
			unsigned match = php_http_negotiate_match(key.str, key.len-1, Z_STRVAL_P(supported), Z_STRLEN_P(supported), sep_str, sep_len);

			if (match > best_match) {
				best_match = match;
				q = val;
			}
		}
	}

	if (q && Z_DVAL_PP(q) > 0) {
		Z_ADDREF_PP(q);
		zend_hash_update(result, Z_STRVAL_P(supported), Z_STRLEN_P(supported) + 1, (void *) q, sizeof(zval *), NULL);
	}

	zval_ptr_dtor(&supported);
	return ZEND_HASH_APPLY_KEEP;
}

HashTable *php_http_negotiate(const char *value_str, size_t value_len, HashTable *supported, const char *primary_sep_str, size_t primary_sep_len TSRMLS_DC)
{
	HashTable *result = NULL;

	if (value_str && value_len) {
		unsigned i = 0;
		zval arr, **val, **arg, **zq;
		HashPosition pos;
		HashTable params;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(1);
		php_http_params_opts_t opts;

		zend_hash_init(&params, 10, NULL, ZVAL_PTR_DTOR, 0);
		php_http_params_opts_default_get(&opts);
		opts.input.str = estrndup(value_str, value_len);
		opts.input.len = value_len;
		php_http_params_parse(&params, &opts TSRMLS_CC);
		efree(opts.input.str);

		INIT_PZVAL(&arr);
		array_init(&arr);

		FOREACH_HASH_KEYVAL(pos, &params, key, val) {
			double q;

			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(val), ZEND_STRS("arguments"), (void *) &arg)
			&&	IS_ARRAY == Z_TYPE_PP(arg)
			&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(arg), ZEND_STRS("q"), (void *) &zq)) {
				zval *tmp = php_http_ztyp(IS_DOUBLE, *zq);

				q = Z_DVAL_P(tmp);
				zval_ptr_dtor(&tmp);
			} else {
				q = 1.0 - ++i / 100.0;
			}

			if (key.type == HASH_KEY_IS_STRING) {
				add_assoc_double_ex(&arr, key.str, key.len, q);
			} else {
				add_index_double(&arr, key.num, q);
			}

			STR_FREE(key.str);
		}

#if 0
		zend_print_zval_r(&arr, 1 TSRMLS_CC);
#endif

		ALLOC_HASHTABLE(result);
		zend_hash_init(result, zend_hash_num_elements(supported), NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_apply_with_arguments(supported TSRMLS_CC, php_http_negotiate_reduce, 4, Z_ARRVAL(arr), result, primary_sep_str, primary_sep_len);
		zend_hash_destroy(&params);
		zval_dtor(&arr);
		zend_hash_sort(result, zend_qsort, php_http_negotiate_sort, 0 TSRMLS_CC);
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


