/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#include "php_http.h"

#include "php_variables.h"
#if HTTP_HAVE_EXT(ICONV)
#	undef PHP_ATOM_INC
#	include "ext/iconv/php_iconv.h"
#	include "ext/standard/url.h"
#endif

#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_querystring_api.h"

#ifdef ZEND_ENGINE_2
#define OBJ_PROP_CE http_querystring_object_ce
extern zend_class_entry *http_querystring_object_ce;
#endif


#define http_querystring_modify_array_ex(q, t, k, kl, i, pe) _http_querystring_modify_array_ex((q), (t), (k), (kl), (i), (pe) TSRMLS_CC)
static inline int _http_querystring_modify_array_ex(zval *qarray, int key_type, char *key, int keylen, ulong idx, zval *params_entry TSRMLS_DC);
#define http_querystring_modify_array(q, p) _http_querystring_modify_array((q), (p) TSRMLS_CC)
static inline int _http_querystring_modify_array(zval *qarray, zval *params TSRMLS_DC);


#if HTTP_HAVE_EXT(ICONV)
PHP_HTTP_API int _http_querystring_xlate(zval *array, zval *param, const char *ie, const char *oe TSRMLS_DC)
{
	HashPosition pos;
	zval **entry = NULL;
	char *xlate_str = NULL, *xkey, *kstr = NULL;
	size_t xlate_len = 0, xlen;
	uint klen = 0;
	ulong kidx = 0;
	
	FOREACH_KEYLENVAL(pos, param, kstr, klen, kidx, entry) {
		if (kstr) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(kstr, klen-1, &xkey, &xlen, oe, ie)) {
				http_error_ex(HE_WARNING, HTTP_E_QUERYSTRING, "Failed to convert '%.*s' from '%s' to '%s'", klen-1, kstr, ie, oe);
				return FAILURE;
			}
		}
		
		if (Z_TYPE_PP(entry) == IS_STRING) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(Z_STRVAL_PP(entry), Z_STRLEN_PP(entry), &xlate_str, &xlate_len, oe, ie)) {
				if (kstr) {
					efree(xkey);
				}
				http_error_ex(HE_WARNING, HTTP_E_QUERYSTRING, "Failed to convert '%.*s' from '%s' to '%s'", Z_STRLEN_PP(entry), Z_STRVAL_PP(entry), ie, oe);
				return FAILURE;
			}
			if (kstr) {
				add_assoc_stringl_ex(array, xkey, xlen+1, xlate_str, xlate_len, 0);
			} else {
				add_index_stringl(array, kidx, xlate_str, xlate_len, 0);
			}
		} else if (Z_TYPE_PP(entry) == IS_ARRAY) {
			zval *subarray;
			
			MAKE_STD_ZVAL(subarray);
			array_init(subarray);
			if (kstr) {
				add_assoc_zval_ex(array, xkey, xlen+1, subarray);
			} else {
				add_index_zval(array, kidx, subarray);
			}
			if (SUCCESS != http_querystring_xlate(subarray, *entry, ie, oe)) {
				if (kstr) {
					efree(xkey);
				}
				return FAILURE;
			}
		}
		
		if (kstr) {
			kstr = NULL;
			efree(xkey);
		}
	}
	return SUCCESS;
}
#endif /* HAVE_ICONV */

PHP_HTTP_API void _http_querystring_update(zval *qarray, zval *qstring TSRMLS_DC)
{
	char *s = NULL;
	size_t l = 0;
	
	if (Z_TYPE_P(qarray) != IS_ARRAY) {
		convert_to_array(qarray);
	}
	if (SUCCESS == http_urlencode_hash_ex(Z_ARRVAL_P(qarray), 0, NULL, 0, &s, &l)) {
		zval_dtor(qstring);
		ZVAL_STRINGL(qstring, s, l, 0);
	} else {
		http_error(HE_WARNING, HTTP_E_QUERYSTRING, "Failed to update query string");
	}
}

PHP_HTTP_API int _http_querystring_modify(zval *qarray, zval *params TSRMLS_DC)
{
	if (Z_TYPE_P(params) == IS_ARRAY) {
		return http_querystring_modify_array(qarray, params);
	} else if (Z_TYPE_P(params) == IS_OBJECT) {
#ifdef ZEND_ENGINE_2
		if (!instanceof_function(Z_OBJCE_P(params), http_querystring_object_ce TSRMLS_CC)) {
#endif
			zval temp_array;
			INIT_ZARR(temp_array, HASH_OF(params));
			return http_querystring_modify_array(qarray, &temp_array);
#ifdef ZEND_ENGINE_2
		}
		return http_querystring_modify_array(qarray, GET_PROP_EX(params, queryArray));
#endif
	} else {
		int rv;
		zval array;
		
		INIT_PZVAL(&array);
		array_init(&array);
		
		ZVAL_ADDREF(params);
		convert_to_string_ex(&params);
		sapi_module.treat_data(PARSE_STRING, estrdup(Z_STRVAL_P(params)), &array TSRMLS_CC);
		zval_ptr_dtor(&params);
		rv = http_querystring_modify_array(qarray, &array);
		zval_dtor(&array);
		return rv;
	}
}

static inline int _http_querystring_modify_array(zval *qarray, zval *params TSRMLS_DC)
{
	int rv = 0;
	char *key = NULL;
	uint keylen = 0;
	ulong idx = 0;
	HashPosition pos;
	zval **params_entry = NULL;
	
	FOREACH_KEYLENVAL(pos, params, key, keylen, idx, params_entry) {
		if (http_querystring_modify_array_ex(qarray, key ? HASH_KEY_IS_STRING : HASH_KEY_IS_LONG, key, keylen, idx, *params_entry)) {
			rv = 1;
		}
		key = NULL;
	}
	
	return rv;
}

static inline int _http_querystring_modify_array_ex(zval *qarray, int key_type, char *key, int keylen, ulong idx, zval *params_entry TSRMLS_DC)
{
	zval **qarray_entry;
	
	/* delete */
	if (Z_TYPE_P(params_entry) == IS_NULL) {
		if (key_type == HASH_KEY_IS_STRING) {
			return (SUCCESS == zend_hash_del(Z_ARRVAL_P(qarray), key, keylen));
		} else {
			return (SUCCESS == zend_hash_index_del(Z_ARRVAL_P(qarray), idx));
		}
	}
	
	/* update */
	if (	((key_type == HASH_KEY_IS_STRING) && (SUCCESS == zend_hash_find(Z_ARRVAL_P(qarray), key, keylen, (void *) &qarray_entry))) ||
			((key_type == HASH_KEY_IS_LONG) && (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(qarray), idx, (void *) &qarray_entry)))) {
		zval equal;
		
		/* recursive */
		if (Z_TYPE_P(params_entry) == IS_ARRAY) {
			return http_querystring_modify_array(*qarray_entry, params_entry);
		}
		/* equal */
		if ((SUCCESS == is_equal_function(&equal, *qarray_entry, params_entry TSRMLS_CC)) && Z_BVAL(equal)) {
			return 0;
		}
	}
	
	/* add */
	ZVAL_ADDREF(params_entry);
	if (key_type == HASH_KEY_IS_STRING) {
		add_assoc_zval_ex(qarray, key, keylen, params_entry);
	} else {
		add_index_zval(qarray, idx, params_entry);
	}
	return 1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
