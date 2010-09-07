/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: php_http_headers_api.h 300300 2010-06-09 07:29:35Z mike $ */

#include "php_http.h"

#ifndef PHP_HTTP_DBG_NEG
#	define PHP_HTTP_DBG_NEG 0
#endif

char *php_http_negotiate_language_func(const char *test, double *quality, HashTable *supported TSRMLS_DC)
{
	zval **value;
	HashPosition pos;
	const char *dash_test;

	FOREACH_HASH_VAL(pos, supported, value) {
#if PHP_HTTP_DBG_NEG
		fprintf(stderr, "strcasecmp('%s', '%s')\n", Z_STRVAL_PP(value), test);
#endif
		if (!strcasecmp(Z_STRVAL_PP(value), test)) {
			return Z_STRVAL_PP(value);
		}
	}

	/* no distinct match found, so try primaries */
	if ((dash_test = strchr(test, '-'))) {
		FOREACH_HASH_VAL(pos, supported, value) {
			int len = dash_test - test;
#if PHP_HTTP_DBG_NEG
			fprintf(stderr, "strncasecmp('%s', '%s', %d)\n", Z_STRVAL_PP(value), test, len);
#endif
			if (	(!strncasecmp(Z_STRVAL_PP(value), test, len)) &&
					(	(Z_STRVAL_PP(value)[len] == '\0') ||
						(Z_STRVAL_PP(value)[len] == '-'))) {
				*quality *= .9;
				return Z_STRVAL_PP(value);
			}
		}
	}

	return NULL;
}


char *php_http_negotiate_default_func(const char *test, double *quality, HashTable *supported TSRMLS_DC)
{
	zval **value;
	HashPosition pos;
	(void) quality;

	FOREACH_HASH_VAL(pos, supported, value) {
#if PHP_HTTP_DBG_NEG
		fprintf(stderr, "strcasecmp('%s', '%s')\n", Z_STRVAL_PP(value), test);
#endif
		if (!strcasecmp(Z_STRVAL_PP(value), test)) {
			return Z_STRVAL_PP(value);
		}
	}

	return NULL;
}


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


PHP_HTTP_API HashTable *php_http_negotiate(const char *value, HashTable *supported, php_http_negotiate_func_t neg TSRMLS_DC)
{
	HashTable *result = NULL;

	if (*value) {
		zval ex_arr, ex_del, ex_val;

		INIT_PZVAL(&ex_del);
		INIT_PZVAL(&ex_arr);
		INIT_PZVAL(&ex_val);
		ZVAL_STRINGL(&ex_del, ",", 1, 0);
		ZVAL_STRING(&ex_val, value, 1);
		array_init(&ex_arr);

		php_explode(&ex_del, &ex_val, &ex_arr, INT_MAX);

		if (zend_hash_num_elements(Z_ARRVAL(ex_arr)) > 0) {
			int i = 0;
			HashPosition pos;
			zval **entry, array;

			INIT_PZVAL(&array);
			array_init(&array);

			if (!neg) {
				neg = php_http_negotiate_default_func;
			}

			FOREACH_HASH_VAL(pos, Z_ARRVAL(ex_arr), entry) {
				int ident_len;
				double quality;
				char *selected, *identifier, *freeme;
				const char *separator;

#if PHP_HTTP_DBG_NEG
				fprintf(stderr, "Checking %s\n", Z_STRVAL_PP(entry));
#endif

				if ((separator = strchr(Z_STRVAL_PP(entry), ';'))) {
					const char *ptr = separator;

					while (*++ptr && !PHP_HTTP_IS_CTYPE(digit, *ptr) && '.' != *ptr);

					quality = zend_strtod(ptr, NULL);
					identifier = estrndup(Z_STRVAL_PP(entry), ident_len = separator - Z_STRVAL_PP(entry));
				} else {
					quality = 1000.0 - i++;
					identifier = estrndup(Z_STRVAL_PP(entry), ident_len = Z_STRLEN_PP(entry));
				}
				freeme = identifier;

				while (PHP_HTTP_IS_CTYPE(space, *identifier)) {
					++identifier;
					--ident_len;
				}
				while (ident_len && PHP_HTTP_IS_CTYPE(space, identifier[ident_len - 1])) {
					identifier[--ident_len] = '\0';
				}

				if ((selected = neg(identifier, &quality, supported TSRMLS_CC))) {
					/* don't overwrite previously set with higher quality */
					if (!zend_hash_exists(Z_ARRVAL(array), selected, strlen(selected) + 1)) {
						add_assoc_double(&array, selected, quality);
					}
				}

				efree(freeme);
			}

			result = Z_ARRVAL(array);
			zend_hash_sort(result, zend_qsort, php_http_negotiate_sort, 0 TSRMLS_CC);
		}

		zval_dtor(&ex_arr);
		zval_dtor(&ex_val);
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


