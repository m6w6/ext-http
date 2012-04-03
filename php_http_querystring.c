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

#include <php_variables.h>
#include <ext/spl/spl_array.h>

#ifdef PHP_HTTP_HAVE_ICONV
#	undef PHP_ATOM_INC
#	include <ext/iconv/php_iconv.h>
#endif


#define QS_MERGE 1

static inline void php_http_querystring_set(zval *instance, zval *params, int flags TSRMLS_DC)
{
	zval *qa;

	if (flags & QS_MERGE) {
		qa = php_http_zsep(1, IS_ARRAY, zend_read_property(php_http_querystring_get_class_entry(), instance, ZEND_STRL("queryArray"), 0 TSRMLS_CC));
	} else {
		MAKE_STD_ZVAL(qa);
		array_init(qa);
	}

	php_http_querystring_update(qa, params, NULL TSRMLS_CC);
	zend_update_property(php_http_querystring_get_class_entry(), instance, ZEND_STRL("queryArray"), qa TSRMLS_CC);
	zval_ptr_dtor(&qa);
}

static inline void php_http_querystring_str(zval *instance, zval *return_value TSRMLS_DC)
{
	zval *qa = zend_read_property(php_http_querystring_get_class_entry(), instance, ZEND_STRL("queryArray"), 0 TSRMLS_CC);

	if (Z_TYPE_P(qa) == IS_ARRAY) {
		php_http_querystring_update(qa, NULL, return_value TSRMLS_CC);
	} else {
		RETURN_EMPTY_STRING();
	}
}

static inline void php_http_querystring_get(zval *this_ptr, int type, char *name, uint name_len, zval *defval, zend_bool del, zval *return_value TSRMLS_DC)
{
	zval **arrval, *qarray = zend_read_property(php_http_querystring_get_class_entry(), getThis(), ZEND_STRL("queryArray"), 0 TSRMLS_CC);

	if ((Z_TYPE_P(qarray) == IS_ARRAY) && (SUCCESS == zend_symtable_find(Z_ARRVAL_P(qarray), name, name_len + 1, (void *) &arrval))) {
		if (type) {
			zval *value = php_http_ztyp(type, *arrval);
			RETVAL_ZVAL(value, 1, 1);
		} else {
			RETVAL_ZVAL(*arrval, 1, 0);
		}

		if (del) {
			zval *delarr;

			MAKE_STD_ZVAL(delarr);
			array_init(delarr);
			add_assoc_null_ex(delarr, name, name_len + 1);
			php_http_querystring_set(this_ptr, delarr, QS_MERGE TSRMLS_CC);
			zval_ptr_dtor(&delarr);
		}
	} else if(defval) {
		RETURN_ZVAL(defval, 1, 0);
	}
}

#ifdef PHP_HTTP_HAVE_ICONV
PHP_HTTP_API STATUS php_http_querystring_xlate(zval *dst, zval *src, const char *ie, const char *oe TSRMLS_DC)
{
	HashPosition pos;
	zval **entry = NULL;
	char *xlate_str = NULL, *xkey;
	size_t xlate_len = 0, xlen;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	
	FOREACH_KEYVAL(pos, src, key, entry) {
		if (key.type == HASH_KEY_IS_STRING) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(key.str, key.len-1, &xkey, &xlen, oe, ie)) {
				php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Failed to convert '%.*s' from '%s' to '%s'", key.len-1, key.str, ie, oe);
				return FAILURE;
			}
		}
		
		if (Z_TYPE_PP(entry) == IS_STRING) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(Z_STRVAL_PP(entry), Z_STRLEN_PP(entry), &xlate_str, &xlate_len, oe, ie)) {
				if (key.type == HASH_KEY_IS_STRING) {
					efree(xkey);
				}
				php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Failed to convert '%.*s' from '%s' to '%s'", Z_STRLEN_PP(entry), Z_STRVAL_PP(entry), ie, oe);
				return FAILURE;
			}
			if (key.type == HASH_KEY_IS_STRING) {
				add_assoc_stringl_ex(dst, xkey, xlen+1, xlate_str, xlate_len, 0);
			} else {
				add_index_stringl(dst, key.num, xlate_str, xlate_len, 0);
			}
		} else if (Z_TYPE_PP(entry) == IS_ARRAY) {
			zval *subarray;
			
			MAKE_STD_ZVAL(subarray);
			array_init(subarray);
			if (key.type == HASH_KEY_IS_STRING) {
				add_assoc_zval_ex(dst, xkey, xlen+1, subarray);
			} else {
				add_index_zval(dst, key.num, subarray);
			}
			if (SUCCESS != php_http_querystring_xlate(subarray, *entry, ie, oe TSRMLS_CC)) {
				if (key.type == HASH_KEY_IS_STRING) {
					efree(xkey);
				}
				return FAILURE;
			}
		}
		
		if (key.type == HASH_KEY_IS_STRING) {
			efree(xkey);
		}
	}
	return SUCCESS;
}
#endif /* HAVE_ICONV */

PHP_HTTP_API STATUS php_http_querystring_ctor(zval *instance, zval *params TSRMLS_DC)
{
	php_http_querystring_set(instance, params, 0 TSRMLS_CC);
	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_querystring_update(zval *qarray, zval *params, zval *outstring TSRMLS_DC)
{
	/* enforce proper type */
	if (Z_TYPE_P(qarray) != IS_ARRAY) {
		convert_to_array(qarray);
	}

	/* modify qarray */
	if (params) {
		HashPosition pos;
		HashTable *ptr;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		zval **params_entry,  **qarray_entry;
		zval zv, *zv_ptr = NULL;

		INIT_PZVAL(&zv);
		ZVAL_NULL(&zv);

		/* squeeze the hash out of the zval */
		if (Z_TYPE_P(params) == IS_OBJECT && instanceof_function(Z_OBJCE_P(params), php_http_querystring_get_class_entry() TSRMLS_CC)) {
			zv_ptr = php_http_ztyp(IS_ARRAY, zend_read_property(php_http_querystring_get_class_entry(), params, ZEND_STRL("queryArray"), 0 TSRMLS_CC));
			ptr = Z_ARRVAL_P(zv_ptr);
		} else if (Z_TYPE_P(params) == IS_OBJECT || Z_TYPE_P(params) == IS_ARRAY) {
			ptr = HASH_OF(params);
		} else {
			zv_ptr = php_http_ztyp(IS_STRING, params);
			array_init(&zv);
			php_default_treat_data(PARSE_STRING, estrdup(Z_STRVAL_P(zv_ptr)), &zv TSRMLS_CC);
			zval_ptr_dtor(&zv_ptr);
			zv_ptr = NULL;
			ptr = Z_ARRVAL(zv);
		}

		FOREACH_HASH_KEYVAL(pos, ptr, key, params_entry) {
			/* only public properties */
			if (key.type != HASH_KEY_IS_STRING || *key.str) {
				if (Z_TYPE_PP(params_entry) == IS_NULL) {
					/*
					 * delete
					 */
					if (key.type == HASH_KEY_IS_STRING) {
						zend_hash_del(Z_ARRVAL_P(qarray), key.str, key.len);
					} else {
						zend_hash_index_del(Z_ARRVAL_P(qarray), key.num);
					}
				} else if (	((key.type == HASH_KEY_IS_STRING) && (SUCCESS == zend_hash_find(Z_ARRVAL_P(qarray), key.str, key.len, (void *) &qarray_entry)))
						||	((key.type == HASH_KEY_IS_LONG) && (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(qarray), key.num, (void *) &qarray_entry)))) {
					/*
					 * update
					 */
					zval equal, *entry = NULL;

					/* recursive */
					if (Z_TYPE_PP(params_entry) == IS_ARRAY || Z_TYPE_PP(params_entry) == IS_OBJECT) {
						entry = php_http_zsep(1, IS_ARRAY, *qarray_entry);
						php_http_querystring_update(entry, *params_entry, NULL TSRMLS_CC);
					} else if ((FAILURE == is_equal_function(&equal, *qarray_entry, *params_entry TSRMLS_CC)) || !Z_BVAL(equal)) {
						Z_ADDREF_PP(params_entry);
						entry = *params_entry;
					}

					if (entry) {
						if (key.type == HASH_KEY_IS_STRING) {
							zend_hash_update(Z_ARRVAL_P(qarray), key.str, key.len, (void *) &entry, sizeof(zval *), NULL);
						} else {
							zend_hash_index_update(Z_ARRVAL_P(qarray), key.num, (void *) &entry, sizeof(zval *), NULL);
						}
					}
				} else {
					zval *entry;
					/*
					 * add
					 */
					if (Z_TYPE_PP(params_entry) == IS_OBJECT) {
						MAKE_STD_ZVAL(entry);
						array_init(entry);
						php_http_querystring_update(entry, *params_entry, NULL TSRMLS_CC);
					} else {
						Z_ADDREF_PP(params_entry);
						entry = *params_entry;
					}
					if (key.type == HASH_KEY_IS_STRING) {
						add_assoc_zval_ex(qarray, key.str, key.len, entry);
					} else {
						add_index_zval(qarray, key.num, entry);
					}
				}
			}
		}
		/* clean up */
		if (zv_ptr) {
			zval_ptr_dtor(&zv_ptr);
		}
		zval_dtor(&zv);
	}

	/* serialize to string */
	if (outstring) {
		char *s;
		size_t l;

		if (SUCCESS == php_http_url_encode_hash(Z_ARRVAL_P(qarray), NULL, 0, &s, &l TSRMLS_CC)) {
			zval_dtor(outstring);
			ZVAL_STRINGL(outstring, s, l, 0);
		} else {
			php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Failed to encode query string");
			return FAILURE;
		}
	}

	return SUCCESS;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpQueryString, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpQueryString, method, 0)
#define PHP_HTTP_QUERYSTRING_ME(method, visibility)		PHP_ME(HttpQueryString, method, PHP_HTTP_ARGS(HttpQueryString, method), visibility)
#define PHP_HTTP_QUERYSTRING_GME(method, visibility)	PHP_ME(HttpQueryString, method, PHP_HTTP_ARGS(HttpQueryString, __getter), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(params, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getGlobalInstance);

PHP_HTTP_EMPTY_ARGS(toArray);
PHP_HTTP_EMPTY_ARGS(toString);

PHP_HTTP_BEGIN_ARGS(get, 0)
	PHP_HTTP_ARG_VAL(name, 0)
	PHP_HTTP_ARG_VAL(type, 0)
	PHP_HTTP_ARG_VAL(defval, 0)
	PHP_HTTP_ARG_VAL(delete, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(set, 1)
	PHP_HTTP_ARG_VAL(params, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(mod, 0)
	PHP_HTTP_ARG_VAL(params, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(__getter, 1)
	PHP_HTTP_ARG_VAL(name, 0)
	PHP_HTTP_ARG_VAL(defval, 0)
	PHP_HTTP_ARG_VAL(delete, 0)
PHP_HTTP_END_ARGS;

#ifdef PHP_HTTP_HAVE_ICONV
PHP_HTTP_BEGIN_ARGS(xlate, 2)
	PHP_HTTP_ARG_VAL(from_encoding, 0)
	PHP_HTTP_ARG_VAL(to_encoding, 0)
PHP_HTTP_END_ARGS;
#endif

PHP_HTTP_EMPTY_ARGS(serialize);
PHP_HTTP_BEGIN_ARGS(unserialize, 1)
	PHP_HTTP_ARG_VAL(serialized, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetGet, 1)
	PHP_HTTP_ARG_VAL(offset, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetSet, 2)
	PHP_HTTP_ARG_VAL(offset, 0)
	PHP_HTTP_ARG_VAL(value, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetExists, 1)
	PHP_HTTP_ARG_VAL(offset, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(offsetUnset, 1)
	PHP_HTTP_ARG_VAL(offset, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getIterator);

static zend_class_entry *php_http_querystring_class_entry;

zend_class_entry *php_http_querystring_get_class_entry(void)
{
	return php_http_querystring_class_entry;
}

static zend_function_entry php_http_querystring_method_entry[] = {
	PHP_HTTP_QUERYSTRING_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	
	PHP_HTTP_QUERYSTRING_ME(toArray, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpQueryString, __toString, toString, PHP_HTTP_ARGS(HttpQueryString, toString), ZEND_ACC_PUBLIC)
	
	PHP_HTTP_QUERYSTRING_ME(get, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(set, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(mod, ZEND_ACC_PUBLIC)
	
	PHP_HTTP_QUERYSTRING_GME(getBool, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_GME(getInt, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_GME(getFloat, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_GME(getString, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_GME(getArray, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_GME(getObject, ZEND_ACC_PUBLIC)
	
	PHP_HTTP_QUERYSTRING_ME(getIterator, ZEND_ACC_PUBLIC)

	PHP_HTTP_QUERYSTRING_ME(getGlobalInstance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#ifdef PHP_HTTP_HAVE_ICONV
	PHP_HTTP_QUERYSTRING_ME(xlate, ZEND_ACC_PUBLIC)
#endif
	
	/* Implements Serializable */
	PHP_HTTP_QUERYSTRING_ME(serialize, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(unserialize, ZEND_ACC_PUBLIC)
	
	/* Implements ArrayAccess */
	PHP_HTTP_QUERYSTRING_ME(offsetGet, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(offsetSet, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(offsetExists, ZEND_ACC_PUBLIC)
	PHP_HTTP_QUERYSTRING_ME(offsetUnset, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_querystring)
{
	PHP_HTTP_REGISTER_CLASS(http, QueryString, http_querystring, php_http_object_get_class_entry(), 0);
	
	zend_class_implements(php_http_querystring_class_entry TSRMLS_CC, 3, zend_ce_serializable, zend_ce_arrayaccess, zend_ce_aggregate);
	
	zend_declare_property_null(php_http_querystring_class_entry, ZEND_STRL("instance"), (ZEND_ACC_STATIC|ZEND_ACC_PRIVATE) TSRMLS_CC);
	zend_declare_property_null(php_http_querystring_class_entry, ZEND_STRL("queryArray"), ZEND_ACC_PRIVATE TSRMLS_CC);
	
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_BOOL"), PHP_HTTP_QUERYSTRING_TYPE_BOOL TSRMLS_CC);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_INT"), PHP_HTTP_QUERYSTRING_TYPE_INT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_FLOAT"), PHP_HTTP_QUERYSTRING_TYPE_FLOAT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_STRING"), PHP_HTTP_QUERYSTRING_TYPE_STRING TSRMLS_CC);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_ARRAY"), PHP_HTTP_QUERYSTRING_TYPE_ARRAY TSRMLS_CC);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_OBJECT"), PHP_HTTP_QUERYSTRING_TYPE_OBJECT TSRMLS_CC);
	
	return SUCCESS;
}

PHP_METHOD(HttpQueryString, __construct)
{
	zval *params = NULL;
	
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &params)) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				php_http_querystring_set(getThis(), params, 0 TSRMLS_CC);
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpQueryString, getGlobalInstance)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				zval *instance = *zend_std_get_static_property(php_http_querystring_class_entry, ZEND_STRL("instance"), 0, NULL TSRMLS_CC);

				if (Z_TYPE_P(instance) != IS_OBJECT) {
					zval **_GET = NULL;

					zend_is_auto_global("_GET", lenof("_GET") TSRMLS_CC);

					if ((SUCCESS == zend_hash_find(&EG(symbol_table), "_GET", sizeof("_GET"), (void *) &_GET))
					&&	(Z_TYPE_PP(_GET) == IS_ARRAY)
					) {
						MAKE_STD_ZVAL(instance);
						ZVAL_OBJVAL(instance, php_http_querystring_object_new(php_http_querystring_class_entry TSRMLS_CC), 0);

						SEPARATE_ZVAL_TO_MAKE_IS_REF(_GET);
						convert_to_array(*_GET);
						zend_update_property(php_http_querystring_class_entry, instance, ZEND_STRL("queryArray"), *_GET TSRMLS_CC);

						zend_update_static_property(php_http_querystring_class_entry, ZEND_STRL("instance"), instance TSRMLS_CC);
						zval_ptr_dtor(&instance);
					} else {
						php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Could not acquire reference to superglobal GET array");
					}
				}
				RETVAL_ZVAL(instance, 1, 0);
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpQueryString, getIterator)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				zval *retval = NULL, *qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0 TSRMLS_CC);

				object_init_ex(return_value, spl_ce_RecursiveArrayIterator);
				zend_call_method_with_1_params(&return_value, spl_ce_RecursiveArrayIterator, NULL, "__construct", &retval, qa);
				if (retval) {
					zval_ptr_dtor(&retval);
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpQueryString, toString)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		RETURN_FALSE;
	}
	php_http_querystring_str(getThis(), return_value TSRMLS_CC);
}

PHP_METHOD(HttpQueryString, toArray)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_querystring_class_entry, "queryArray");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpQueryString, get)
{
	char *name_str = NULL;
	int name_len = 0;
	long type = 0;
	zend_bool del = 0;
	zval *ztype = NULL, *defval = NULL;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|szzb", &name_str, &name_len, &ztype, &defval, &del)) {
		if (name_str && name_len) {
			if (ztype) {
				if (Z_TYPE_P(ztype) == IS_LONG) {
					type = Z_LVAL_P(ztype);
				} else if(Z_TYPE_P(ztype) == IS_STRING) {
					switch (Z_STRVAL_P(ztype)[0]) {
						case 'B': 
						case 'b':	type = PHP_HTTP_QUERYSTRING_TYPE_BOOL;		break;
						case 'L':
						case 'l':
						case 'I':
						case 'i':	type = PHP_HTTP_QUERYSTRING_TYPE_INT;		break;
						case 'd':
						case 'D':
						case 'F':
						case 'f':	type = PHP_HTTP_QUERYSTRING_TYPE_FLOAT;		break;	
						case 'S':
						case 's':	type = PHP_HTTP_QUERYSTRING_TYPE_STRING;	break;
						case 'A':
						case 'a':	type = PHP_HTTP_QUERYSTRING_TYPE_ARRAY;		break;
						case 'O':
						case 'o':	type = PHP_HTTP_QUERYSTRING_TYPE_OBJECT;	break;
					}
				}
			}
			php_http_querystring_get(getThis(), type, name_str, name_len, defval, del, return_value TSRMLS_CC);
		} else {
			php_http_querystring_str(getThis(), return_value TSRMLS_CC);
		}
	}
}

PHP_METHOD(HttpQueryString, set)
{
	zval *params;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
		RETURN_FALSE;
	}
	
	php_http_querystring_set(getThis(), params, QS_MERGE TSRMLS_CC);
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpQueryString, mod)
{
	zval *params;
	
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				ZVAL_OBJVAL(return_value, Z_OBJ_HT_P(getThis())->clone_obj(getThis() TSRMLS_CC), 0);
				php_http_querystring_set(return_value, params, QS_MERGE TSRMLS_CC);
			} end_error_handling();
		}
	} end_error_handling();
}

#define PHP_HTTP_QUERYSTRING_GETTER(method, TYPE) \
PHP_METHOD(HttpQueryString, method) \
{ \
	char *name; \
	int name_len; \
	zval *defval = NULL; \
	zend_bool del = 0; \
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|zb", &name, &name_len, &defval, &del)) { \
		php_http_querystring_get(getThis(), TYPE, name, name_len, defval, del, return_value TSRMLS_CC); \
	} \
}
PHP_HTTP_QUERYSTRING_GETTER(getBool, IS_BOOL);
PHP_HTTP_QUERYSTRING_GETTER(getInt, IS_LONG);
PHP_HTTP_QUERYSTRING_GETTER(getFloat, IS_DOUBLE);
PHP_HTTP_QUERYSTRING_GETTER(getString, IS_STRING);
PHP_HTTP_QUERYSTRING_GETTER(getArray, IS_ARRAY);
PHP_HTTP_QUERYSTRING_GETTER(getObject, IS_OBJECT);

#ifdef PHP_HTTP_HAVE_ICONV
PHP_METHOD(HttpQueryString, xlate)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		char *ie, *oe;
		int ie_len, oe_len;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &ie, &ie_len, &oe, &oe_len)) {
			with_error_handling(EH_THROW,  php_http_exception_get_class_entry()) {
				zval *na, *qa = php_http_ztyp(IS_ARRAY, zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0 TSRMLS_CC));

				MAKE_STD_ZVAL(na);
				array_init(na);
				if (SUCCESS == php_http_querystring_xlate(na, qa, ie, oe TSRMLS_CC)) {
					php_http_querystring_set(getThis(), na, 0 TSRMLS_CC);
				}

				zval_ptr_dtor(&na);
				zval_ptr_dtor(&qa);

				RETVAL_ZVAL(getThis(), 1, 0);
			} end_error_handling();
		}
	} end_error_handling();
	
}
#endif /* HAVE_ICONV */

PHP_METHOD(HttpQueryString, serialize)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		RETURN_FALSE;
	}
	php_http_querystring_str(getThis(), return_value TSRMLS_CC);
}

PHP_METHOD(HttpQueryString, unserialize)
{
	zval *serialized;
	
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &serialized)) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				if (Z_TYPE_P(serialized) == IS_STRING) {
					php_http_querystring_set(getThis(), serialized, 0 TSRMLS_CC);
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_QUERYSTRING, "Expected a string as parameter");
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpQueryString, offsetGet)
{
	char *offset_str;
	int offset_len;
	zval **value;
	
	if ((SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len))) {
		zval *qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0 TSRMLS_CC);

		if (Z_TYPE_P(qa) == IS_ARRAY
		&&	SUCCESS == zend_symtable_find(Z_ARRVAL_P(qa), offset_str, offset_len + 1, (void *) &value)
		) {
			RETVAL_ZVAL(*value, 1, 0);
		}
	}
}

PHP_METHOD(HttpQueryString, offsetSet)
{
	char *offset_str;
	int offset_len;
	zval *value;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &offset_str, &offset_len, &value)) {
		zval *param;
		
		MAKE_STD_ZVAL(param);
		array_init(param);
		Z_ADDREF_P(value);
		add_assoc_zval_ex(param, offset_str, offset_len + 1, value);
		php_http_querystring_set(getThis(), param, 0 TSRMLS_CC);
		zval_ptr_dtor(&param);
	}
}

PHP_METHOD(HttpQueryString, offsetExists)
{
	char *offset_str;
	int offset_len;
	zval **value;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len)) {
		zval *qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0 TSRMLS_CC);

		if (Z_TYPE_P(qa) == IS_ARRAY
		&&	SUCCESS == zend_symtable_find(Z_ARRVAL_P(qa), offset_str, offset_len + 1, (void *) &value)
		&&	Z_TYPE_PP(value) != IS_NULL
		) {
			RETURN_TRUE;
		}
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpQueryString, offsetUnset)
{
	char *offset_str;
	int offset_len;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len)) {
		zval *param;
		
		MAKE_STD_ZVAL(param);
		array_init(param);
		add_assoc_null_ex(param, offset_str, offset_len + 1);
		php_http_querystring_set(getThis(), param, QS_MERGE TSRMLS_CC);
		zval_ptr_dtor(&param);
	}
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
