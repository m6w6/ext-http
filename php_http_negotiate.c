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

#include "php_http.h"

#include <ext/standard/php_string.h>

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

static int php_http_negotiate_reduce(void *p TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key)
{
	char *tmp;
	zval **q, *supported = php_http_ztyp(IS_STRING, *(zval **)p);
	HashTable *params = va_arg(args, HashTable *);
	HashTable *result = va_arg(args, HashTable *);

	tmp = php_strtolower(estrndup(Z_STRVAL_P(supported), Z_STRLEN_P(supported)), Z_STRLEN_P(supported));
	if (SUCCESS == zend_symtable_find(params, tmp, Z_STRLEN_P(supported) + 1, (void *) &q)) {
		Z_ADDREF_PP(q);
		zend_symtable_update(result, Z_STRVAL_P(supported), Z_STRLEN_P(supported) + 1, (void *) q, sizeof(zval *), NULL);
	}
	efree(tmp);
	zval_ptr_dtor(&supported);
	return ZEND_HASH_APPLY_KEEP;
}

PHP_HTTP_API HashTable *php_http_negotiate(const char *value_str, size_t value_len, HashTable *supported, const char *primary_sep_str, size_t primary_sep_len TSRMLS_DC)
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
				const char *ptr;

				php_strtolower(key.str, key.len - 1);
				add_assoc_double_ex(&arr, key.str, key.len, q);

				if (primary_sep_str && primary_sep_len && (ptr = php_http_locate_str(key.str, key.len - 1, primary_sep_str, primary_sep_len))) {
					key.str[ptr - key.str] = '\0';
					add_assoc_double_ex(&arr, key.str, ptr - key.str + 1, q - i / 1000.0);
				}
			} else {
				add_index_double(&arr, key.num, q);
			}

			STR_FREE(key.str);
		}

		ALLOC_HASHTABLE(result);
		zend_hash_init(result, zend_hash_num_elements(supported), NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_apply_with_arguments(supported TSRMLS_CC, php_http_negotiate_reduce, 2, Z_ARRVAL(arr), result);
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


