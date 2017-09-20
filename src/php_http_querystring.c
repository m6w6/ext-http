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

#include "php_variables.h"
#include "ext/spl/spl_array.h"

#if PHP_HTTP_HAVE_ICONV
#	ifndef HAVE_ICONV
#		define HAVE_ICONV 1
#	endif
#	undef PHP_ATOM_INC
#	include "ext/iconv/php_iconv.h"
#endif

static zend_class_entry *php_http_querystring_class_entry;
zend_class_entry *php_http_querystring_get_class_entry(void)
{
	return php_http_querystring_class_entry;
}

#define QS_MERGE 1

static inline void php_http_querystring_set(zval *instance, zval *params, int flags)
{
	zval qa;

	array_init(&qa);

	if (flags & QS_MERGE) {
		zval old_tmp, *old = zend_read_property(php_http_querystring_class_entry, instance, ZEND_STRL("queryArray"), 0, &old_tmp);

		ZVAL_DEREF(old);
		if (Z_TYPE_P(old) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old), Z_ARRVAL(qa));
		}
	}

	php_http_querystring_update(&qa, params, NULL);
	zend_update_property(php_http_querystring_class_entry, instance, ZEND_STRL("queryArray"), &qa);
	zval_ptr_dtor(&qa);
}

static inline void php_http_querystring_str(zval *instance, zval *return_value)
{
	zval qa_tmp, *qa = zend_read_property(php_http_querystring_class_entry, instance, ZEND_STRL("queryArray"), 0, &qa_tmp);

	ZVAL_DEREF(qa);
	if (Z_TYPE_P(qa) == IS_ARRAY) {
		php_http_querystring_update(qa, NULL, return_value);
	} else {
		RETURN_EMPTY_STRING();
	}
}

static inline void php_http_querystring_get(zval *instance, int type, char *name, uint name_len, zval *defval, zend_bool del, zval *return_value)
{
	zval *arrval, qarray_tmp, *qarray = zend_read_property(php_http_querystring_class_entry, instance, ZEND_STRL("queryArray"), 0, &qarray_tmp);

	ZVAL_DEREF(qarray);
	if ((Z_TYPE_P(qarray) == IS_ARRAY) && (arrval = zend_symtable_str_find(Z_ARRVAL_P(qarray), name, name_len))) {
		if (type && type != Z_TYPE_P(arrval)) {
			zval tmp;

			ZVAL_DUP(&tmp, arrval);
			convert_to_explicit_type(&tmp, type);
			RETVAL_ZVAL(&tmp, 0, 0);
		} else {
			RETVAL_ZVAL(arrval, 1, 0);
		}

		if (del) {
			zval delarr;

			array_init(&delarr);
			add_assoc_null_ex(&delarr, name, name_len);
			php_http_querystring_set(instance, &delarr, QS_MERGE);
			zval_ptr_dtor(&delarr);
		}
	} else if(defval) {
		RETURN_ZVAL(defval, 1, 0);
	}
}

#if PHP_HTTP_HAVE_ICONV
ZEND_RESULT_CODE php_http_querystring_xlate(zval *dst, zval *src, const char *ie, const char *oe)
{
	zval *entry;
	zend_string *xkey, *xstr;
	php_http_arrkey_t key;
	
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(src), key.h, key.key, entry)
	{
		if (key.key) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(key.key->val, key.key->len, &xkey, oe, ie)) {
				php_error_docref(NULL, E_WARNING, "Failed to convert '%.*s' from '%s' to '%s'", key.key->len, key.key->val, ie, oe);
				return FAILURE;
			}
		}
		
		if (Z_TYPE_P(entry) == IS_STRING) {
			if (PHP_ICONV_ERR_SUCCESS != php_iconv_string(Z_STRVAL_P(entry), Z_STRLEN_P(entry), &xstr, oe, ie)) {
				if (key.key) {
					zend_string_release(xkey);
				}
				php_error_docref(NULL, E_WARNING, "Failed to convert '%.*s' from '%s' to '%s'", Z_STRLEN_P(entry), Z_STRVAL_P(entry), ie, oe);
				return FAILURE;
			}
			if (key.key) {
				add_assoc_str_ex(dst, xkey->val, xkey->len, xstr);
			} else {
				add_index_str(dst, key.h, xstr);
			}
		} else if (Z_TYPE_P(entry) == IS_ARRAY) {
			zval subarray;
			
			array_init(&subarray);
			if (key.key) {
				add_assoc_zval_ex(dst, xkey->val, xkey->len, &subarray);
			} else {
				add_index_zval(dst, key.h, &subarray);
			}
			if (SUCCESS != php_http_querystring_xlate(&subarray, entry, ie, oe)) {
				if (key.key) {
					zend_string_release(xkey);
				}
				return FAILURE;
			}
		}
		
		if (key.key) {
			zend_string_release(xkey);
		}
	}
	ZEND_HASH_FOREACH_END();

	return SUCCESS;
}
#endif /* HAVE_ICONV */

ZEND_RESULT_CODE php_http_querystring_ctor(zval *instance, zval *params)
{
	php_http_querystring_set(instance, params, 0);
	return SUCCESS;
}

static int apply_querystring(zval *val)
{
	if (Z_TYPE_P(val) == IS_ARRAY) {
		zval *zvalue;

		if ((zvalue = zend_hash_str_find(Z_ARRVAL_P(val), ZEND_STRL("value")))) {
			zval tmp = {0};

			ZVAL_COPY(&tmp, zvalue);
			zval_dtor(val);
			ZVAL_COPY_VALUE(val, &tmp);
		}
	}

	return ZEND_HASH_APPLY_KEEP;
}

static int apply_querystring_filter(zval *val)
{
	switch (Z_TYPE_P(val)) {
	case IS_NULL:
		return ZEND_HASH_APPLY_REMOVE;
	case IS_ARRAY:
	case IS_OBJECT:
		zend_hash_apply(HASH_OF(val), apply_querystring_filter);
		if (!zend_hash_num_elements(HASH_OF(val))) {
			return ZEND_HASH_APPLY_REMOVE;
		}
		break;
	default:
		break;
	}

	return ZEND_HASH_APPLY_KEEP;
}

ZEND_RESULT_CODE php_http_querystring_parse(HashTable *ht, const char *str, size_t len)
{
	ZEND_RESULT_CODE rv = FAILURE;
	php_http_params_opts_t opts;
	php_http_params_token_t psep = { ZEND_STRL("&") }, *psepp[] = { &psep, NULL };
	php_http_params_token_t vsep = { ZEND_STRL("=") }, *vsepp[] = { &vsep, NULL };
	const char *asi_str = NULL;
	size_t asi_len = 0;

	opts.input.str = estrndup(str, len);
	opts.input.len = len;
	opts.param = psepp;
	opts.arg = NULL;
	opts.val = vsepp;
	opts.flags = PHP_HTTP_PARAMS_QUERY;

	if (SUCCESS == php_http_ini_entry(ZEND_STRL("arg_separator.input"), &asi_str, &asi_len, 0) && asi_len) {
		zval arr;

		array_init_size(&arr, asi_len);

		do {
			add_next_index_stringl(&arr, asi_str++, 1);
		} while (*asi_str);

		opts.param = php_http_params_separator_init(&arr);
		zval_ptr_dtor(&arr);
	}

	ZVAL_TRUE(&opts.defval);

	if (php_http_params_parse(ht, &opts)) {
		zend_hash_apply(ht, apply_querystring);
		rv = SUCCESS;
	}

	if (asi_len) {
		php_http_params_separator_free(opts.param);
	}

	zval_ptr_dtor(&opts.defval);
	efree(opts.input.str);
	return rv;
}

ZEND_RESULT_CODE php_http_querystring_update(zval *qarray, zval *params, zval *outstring)
{
	/* enforce proper type */
	if (Z_TYPE_P(qarray) != IS_ARRAY) {
		convert_to_array(qarray);
	}

	/* modify qarray */
	if (!params) {
		zend_hash_apply(Z_ARRVAL_P(qarray), apply_querystring_filter);
	} else {
		HashTable *ht;
		php_http_arrkey_t key;
		zval zv, *params_entry, *qarray_entry;

		ZVAL_NULL(&zv);

		/* squeeze the hash out of the zval */
		if (Z_TYPE_P(params) == IS_OBJECT && instanceof_function(Z_OBJCE_P(params), php_http_querystring_class_entry)) {
			zval qa_tmp, *qa = zend_read_property(php_http_querystring_class_entry, params, ZEND_STRL("queryArray"), 0, &qa_tmp);

			ZVAL_DEREF(qa);
			convert_to_array(qa);
			ht = Z_ARRVAL_P(qa);
		} else if (Z_TYPE_P(params) == IS_OBJECT || Z_TYPE_P(params) == IS_ARRAY) {
			ht = HASH_OF(params);
		} else {
			zend_string *zs = zval_get_string(params);

			array_init(&zv);
			php_http_querystring_parse(Z_ARRVAL(zv), zs->val, zs->len);
			zend_string_release(zs);

			ht = Z_ARRVAL(zv);
		}

		ZEND_HASH_FOREACH_KEY_VAL_IND(ht, key.h, key.key, params_entry)
		{
			/* only public properties */
			if (!key.key || *key.key->val) {
				if (Z_TYPE_P(params_entry) == IS_NULL) {
					/*
					 * delete
					 */
					if (key.key) {
						zend_hash_del(Z_ARRVAL_P(qarray), key.key);
					} else {
						zend_hash_index_del(Z_ARRVAL_P(qarray), key.h);
					}
				} else if (	((key.key) && (qarray_entry = zend_hash_find(Z_ARRVAL_P(qarray), key.key)))
						||	((!key.key) && (qarray_entry = zend_hash_index_find(Z_ARRVAL_P(qarray), key.h)))) {
					/*
					 * update
					 */
					zval equal, tmp, *entry = NULL;

					ZVAL_UNDEF(&tmp);
					/* recursive */
					if (Z_TYPE_P(params_entry) == IS_ARRAY || Z_TYPE_P(params_entry) == IS_OBJECT) {
						ZVAL_DUP(&tmp, qarray_entry);
						convert_to_array(&tmp);
						php_http_querystring_update(&tmp, params_entry, NULL);
						entry = &tmp;
					} else if ((FAILURE == is_identical_function(&equal, qarray_entry, params_entry)) || Z_TYPE(equal) != IS_TRUE) {
						Z_TRY_ADDREF_P(params_entry);
						entry = params_entry;
					}

					if (key.key) {
						zend_hash_update(Z_ARRVAL_P(qarray), key.key, entry ? entry : &tmp);
					} else {
						zend_hash_index_update(Z_ARRVAL_P(qarray), key.h, entry ? entry : &tmp);
					}
				} else {
					zval entry, *entry_ptr = &entry;
					/*
					 * add
					 */
					if (Z_TYPE_P(params_entry) == IS_OBJECT) {
						array_init(&entry);
						php_http_querystring_update(&entry, params_entry, NULL);
					} else {
						Z_TRY_ADDREF_P(params_entry);
						entry_ptr = params_entry;
					}
					if (key.key) {
						add_assoc_zval_ex(qarray, key.key->val, key.key->len, entry_ptr);
					} else {
						add_index_zval(qarray, key.h, entry_ptr);
					}
				}
			}
		}
		ZEND_HASH_FOREACH_END();

		zval_dtor(&zv);
	}

	/* serialize to string */
	if (outstring) {
		char *s;
		size_t l;

		if (SUCCESS == php_http_url_encode_hash(Z_ARRVAL_P(qarray), NULL, 0, &s, &l)) {
			zval_dtor(outstring);
			ZVAL_STR(outstring, php_http_cs2zs(s, l));
		} else {
			php_error_docref(NULL, E_WARNING, "Failed to encode query string");
			return FAILURE;
		}
	}

	return SUCCESS;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, __construct)
{
	zval *params = NULL;
	zend_error_handling zeh;
	
	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &params), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_bad_querystring_class_entry(), &zeh);
	php_http_querystring_set(getThis(), params, 0);
	zend_restore_error_handling(&zeh);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_getGlobalInstance, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, getGlobalInstance)
{
	zval *instance, *_GET;
	zend_string *zs;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	zs = zend_string_init(ZEND_STRL("instance"), 0);
	instance = zend_std_get_static_property(php_http_querystring_class_entry, zs, 0);
	zend_string_release(zs);

	if (Z_TYPE_P(instance) == IS_OBJECT) {
		RETVAL_ZVAL(instance, 1, 0);
	} else if ((_GET = php_http_env_get_superglobal(ZEND_STRL("_GET")))) {
		ZVAL_OBJ(return_value, php_http_querystring_object_new(php_http_querystring_class_entry));

		ZVAL_MAKE_REF(_GET);
		zend_update_property(php_http_querystring_class_entry, return_value, ZEND_STRL("queryArray"), _GET);
	} else {
		php_http_throw(unexpected_val, "Could not acquire reference to superglobal GET array", NULL);
	}

}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_getIterator, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, getIterator)
{
	zval qa_tmp, *qa;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0, &qa_tmp);

	object_init_ex(return_value, spl_ce_RecursiveArrayIterator);
	zend_call_method_with_1_params(return_value, spl_ce_RecursiveArrayIterator, NULL, "__construct", NULL, qa);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_toString, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, toString)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}
	php_http_querystring_str(getThis(), return_value);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, toArray)
{
	zval zqa_tmp, *zqa;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	zqa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0, &zqa_tmp);
	RETURN_ZVAL(zqa, 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_get, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, defval)
	ZEND_ARG_INFO(0, delete)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, get)
{
	char *name_str = NULL;
	size_t name_len = 0;
	zend_long type = 0;
	zend_bool del = 0;
	zval *ztype = NULL, *defval = NULL;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|szzb", &name_str, &name_len, &ztype, &defval, &del)) {
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
			php_http_querystring_get(getThis(), type, name_str, name_len, defval, del, return_value);
		} else {
			php_http_querystring_str(getThis(), return_value);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_set, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, set)
{
	zval *params;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "z", &params)) {
		return;
	}
	
	php_http_querystring_set(getThis(), params, QS_MERGE);
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_mod, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, mod)
{
	zval qa_tmp, *params, *instance = getThis();
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &params), invalid_arg, return);
	
	zend_replace_error_handling(EH_THROW, php_http_get_exception_bad_querystring_class_entry(), &zeh);
	ZVAL_OBJ(return_value, Z_OBJ_HT_P(instance)->clone_obj(instance));
	/* make sure we do not inherit the reference to _GET */
	SEPARATE_ZVAL(zend_read_property(Z_OBJCE_P(return_value), return_value, ZEND_STRL("queryArray"), 0, &qa_tmp));
	php_http_querystring_set(return_value, params, QS_MERGE);
	zend_restore_error_handling(&zeh);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString___getter, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, defval)
	ZEND_ARG_INFO(0, delete)
ZEND_END_ARG_INFO();
#define PHP_HTTP_QUERYSTRING_GETTER(method, TYPE) \
PHP_METHOD(HttpQueryString, method) \
{ \
	char *name; \
	size_t name_len; \
	zval *defval = NULL; \
	zend_bool del = 0; \
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|zb", &name, &name_len, &defval, &del)) { \
		php_http_querystring_get(getThis(), TYPE, name, name_len, defval, del, return_value); \
	} \
}
PHP_HTTP_QUERYSTRING_GETTER(getBool, _IS_BOOL);
PHP_HTTP_QUERYSTRING_GETTER(getInt, IS_LONG);
PHP_HTTP_QUERYSTRING_GETTER(getFloat, IS_DOUBLE);
PHP_HTTP_QUERYSTRING_GETTER(getString, IS_STRING);
PHP_HTTP_QUERYSTRING_GETTER(getArray, IS_ARRAY);
PHP_HTTP_QUERYSTRING_GETTER(getObject, IS_OBJECT);

#if PHP_HTTP_HAVE_ICONV
ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_xlate, 0, 0, 2)
	ZEND_ARG_INFO(0, from_encoding)
	ZEND_ARG_INFO(0, to_encoding)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, xlate)
{
	char *ie, *oe;
	size_t ie_len, oe_len;
	zval na, qa_tmp, *qa;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &ie, &ie_len, &oe, &oe_len), invalid_arg, return);

	array_init(&na);
	qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0, &qa_tmp);
	ZVAL_DEREF(qa);
	convert_to_array(qa);

	php_http_expect(SUCCESS == php_http_querystring_xlate(&na, qa, ie, oe), bad_conversion,
			zval_ptr_dtor(&na);
			return;
	);

	php_http_querystring_set(getThis(), &na, 0);
	RETVAL_ZVAL(getThis(), 1, 0);
	
	zval_ptr_dtor(&na);
}
#endif /* HAVE_ICONV */

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_serialize, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, serialize)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}
	php_http_querystring_str(getThis(), return_value);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_unserialize, 0, 0, 1)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, unserialize)
{
	zval *serialized;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "z", &serialized)) {
		return;
	}

	if (Z_TYPE_P(serialized) == IS_STRING) {
		php_http_querystring_set(getThis(), serialized, 0);
	} else {
		php_error_docref(NULL, E_WARNING, "Expected a string as parameter");
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_offsetGet, 0, 0, 1)
	ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, offsetGet)
{
	zend_string *offset;
	zval *value, qa_tmp, *qa;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &offset)) {
		return;
	}
	
	qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0, &qa_tmp);
	ZVAL_DEREF(qa);

	if (Z_TYPE_P(qa) == IS_ARRAY) {
		if ((value = zend_symtable_find(Z_ARRVAL_P(qa), offset))) {
			RETVAL_ZVAL(value, 1, 0);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_offsetSet, 0, 0, 2)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, offsetSet)
{
	zend_string *offset;
	zval *value, param, znull;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &offset, &value)) {
		return;
	}

	array_init_size(&param, 1);
	/* unset first */
	ZVAL_NULL(&znull);
	zend_symtable_update(Z_ARRVAL(param), offset, &znull);
	php_http_querystring_set(getThis(), &param, QS_MERGE);
	/* then update, else QS_MERGE would merge sub-arrrays */
	Z_TRY_ADDREF_P(value);
	zend_symtable_update(Z_ARRVAL(param), offset, value);
	php_http_querystring_set(getThis(), &param, QS_MERGE);
	zval_ptr_dtor(&param);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_offsetExists, 0, 0, 1)
	ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, offsetExists)
{
	zend_string *offset;
	zval *value, qa_tmp, *qa;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &offset)) {
		return;
	}
	
	qa = zend_read_property(php_http_querystring_class_entry, getThis(), ZEND_STRL("queryArray"), 0, &qa_tmp);
	ZVAL_DEREF(qa);

	if (Z_TYPE_P(qa) == IS_ARRAY) {
		if ((value = zend_symtable_find(Z_ARRVAL_P(qa), offset))) {
			RETURN_BOOL(Z_TYPE_P(value) != IS_NULL);
		}
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpQueryString_offsetUnset, 0, 0, 1)
	ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpQueryString, offsetUnset)
{
	zend_string *offset;
	zval param, znull;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "S", &offset)) {
		return;
	}

	array_init(&param);
	ZVAL_NULL(&znull);
	zend_symtable_update(Z_ARRVAL(param), offset, &znull);
	php_http_querystring_set(getThis(), &param, QS_MERGE);
	zval_ptr_dtor(&param);
}

static zend_function_entry php_http_querystring_methods[] = {
	PHP_ME(HttpQueryString, __construct, ai_HttpQueryString___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)

	PHP_ME(HttpQueryString, toArray, ai_HttpQueryString_toArray, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, toString, ai_HttpQueryString_toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpQueryString, __toString, toString, ai_HttpQueryString_toString, ZEND_ACC_PUBLIC)

	PHP_ME(HttpQueryString, get, ai_HttpQueryString_get, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, set, ai_HttpQueryString_set, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, mod, ai_HttpQueryString_mod, ZEND_ACC_PUBLIC)

	PHP_ME(HttpQueryString, getBool, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, getInt, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, getFloat, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, getString, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, getArray, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, getObject, ai_HttpQueryString___getter, ZEND_ACC_PUBLIC)

	PHP_ME(HttpQueryString, getIterator, ai_HttpQueryString_getIterator, ZEND_ACC_PUBLIC)

	PHP_ME(HttpQueryString, getGlobalInstance, ai_HttpQueryString_getGlobalInstance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#if PHP_HTTP_HAVE_ICONV
	PHP_ME(HttpQueryString, xlate, ai_HttpQueryString_xlate, ZEND_ACC_PUBLIC)
#endif

	/* Implements Serializable */
	PHP_ME(HttpQueryString, serialize, ai_HttpQueryString_serialize, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, unserialize, ai_HttpQueryString_unserialize, ZEND_ACC_PUBLIC)

	/* Implements ArrayAccess */
	PHP_ME(HttpQueryString, offsetGet, ai_HttpQueryString_offsetGet, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, offsetSet, ai_HttpQueryString_offsetSet, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, offsetExists, ai_HttpQueryString_offsetExists, ZEND_ACC_PUBLIC)
	PHP_ME(HttpQueryString, offsetUnset, ai_HttpQueryString_offsetUnset, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_querystring)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "QueryString", php_http_querystring_methods);
	php_http_querystring_class_entry = zend_register_internal_class(&ce);
	php_http_querystring_class_entry->create_object = php_http_querystring_object_new;
	zend_class_implements(php_http_querystring_class_entry, 3, zend_ce_serializable, zend_ce_arrayaccess, zend_ce_aggregate);

	zend_declare_property_null(php_http_querystring_class_entry, ZEND_STRL("instance"), (ZEND_ACC_STATIC|ZEND_ACC_PRIVATE));
	zend_declare_property_null(php_http_querystring_class_entry, ZEND_STRL("queryArray"), ZEND_ACC_PRIVATE);

	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_BOOL"), PHP_HTTP_QUERYSTRING_TYPE_BOOL);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_INT"), PHP_HTTP_QUERYSTRING_TYPE_INT);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_FLOAT"), PHP_HTTP_QUERYSTRING_TYPE_FLOAT);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_STRING"), PHP_HTTP_QUERYSTRING_TYPE_STRING);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_ARRAY"), PHP_HTTP_QUERYSTRING_TYPE_ARRAY);
	zend_declare_class_constant_long(php_http_querystring_class_entry, ZEND_STRL("TYPE_OBJECT"), PHP_HTTP_QUERYSTRING_TYPE_OBJECT);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
