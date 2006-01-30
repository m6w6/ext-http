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

#include "php_http.h"

#ifdef ZEND_ENGINE_2

#include "zend_interfaces.h"

#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_querystring_object.h"
#include "php_http_exception_object.h"

#define HTTP_BEGIN_ARGS(method, ret_ref, req_args) 	HTTP_BEGIN_ARGS_EX(HttpQueryString, method, ret_ref, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)			HTTP_EMPTY_ARGS_EX(HttpQueryString, method, ret_ref)
#define HTTP_QUERYSTRING_ME(method, visibility)		PHP_ME(HttpQueryString, method, HTTP_ARGS(HttpQueryString, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0, 0)
	HTTP_ARG_VAL(global, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(getInstance, 0, 0)
	HTTP_ARG_VAL(global, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(__toString, 0);

HTTP_BEGIN_ARGS(get, 0, 0)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(type, 0)
	HTTP_ARG_VAL(defval, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(set, 0, 2)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(value, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(del, 0, 1)
	HTTP_ARG_VAL(params, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(mod, 0, 1)
	HTTP_ARG_VAL(params, 0)
HTTP_END_ARGS;

#define http_querystring_object_declare_default_properties() _http_querystring_object_declare_default_properties(TSRMLS_C)
static inline void _http_querystring_object_declare_default_properties(TSRMLS_D);

#define GET_STATIC_PROP(n) *GET_STATIC_PROP_EX(http_querystring_object_ce, n)
#define SET_STATIC_PROP(n, v) SET_STATIC_PROP_EX(http_querystring_object_ce, n, v)
#define OBJ_PROP_CE http_querystring_object_ce
zend_class_entry *http_querystring_object_ce;
zend_function_entry http_querystring_object_fe[] = {
	HTTP_QUERYSTRING_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	HTTP_QUERYSTRING_ME(__toString, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(get, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(set, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(del, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(mod, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(getInstance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_querystring_object_handlers;

PHP_MINIT_FUNCTION(http_querystring_object)
{
	HTTP_REGISTER_CLASS_EX(HttpQueryString, http_querystring_object, NULL, 0);
	
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_BOOL", HTTP_QUERYSTRING_TYPE_BOOL);
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_INT", HTTP_QUERYSTRING_TYPE_INT);
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_FLOAT", HTTP_QUERYSTRING_TYPE_FLOAT);
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_STRING", HTTP_QUERYSTRING_TYPE_STRING);
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_ARRAY", HTTP_QUERYSTRING_TYPE_ARRAY);
	HTTP_LONG_CONSTANT("HTTP_QUERYSTRING_TYPE_OBJECT", HTTP_QUERYSTRING_TYPE_OBJECT);
	
	return SUCCESS;
}

zend_object_value _http_querystring_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_querystring_object_new_ex(ce, NULL);
}

zend_object_value _http_querystring_object_new_ex(zend_class_entry *ce, http_querystring_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_querystring_object *o;

	o = ecalloc(1, sizeof(http_querystring_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_querystring_object, o);
	ov.handlers = &http_querystring_object_handlers;

	return ov;
}

static inline void _http_querystring_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_querystring_object_ce;

	DCL_STATIC_PROP_N(PRIVATE, instance);
	
	DCL_PROP_N(PRIVATE, queryArray);
	DCL_PROP(PRIVATE, string, queryString, "");
	
#ifndef WONKY
	DCL_CONST(long, "TYPE_BOOL", HTTP_QUERYSTRING_TYPE_BOOL);
	DCL_CONST(long, "TYPE_INT", HTTP_QUERYSTRING_TYPE_INT);
	DCL_CONST(long, "TYPE_FLOAT", HTTP_QUERYSTRING_TYPE_FLOAT);
	DCL_CONST(long, "TYPE_STRING", HTTP_QUERYSTRING_TYPE_STRING);
	DCL_CONST(long, "TYPE_ARRAY", HTTP_QUERYSTRING_TYPE_ARRAY);
	DCL_CONST(long, "TYPE_OBJECT", HTTP_QUERYSTRING_TYPE_OBJECT);
#endif
}

void _http_querystring_object_free(zend_object *object TSRMLS_DC)
{
	http_querystring_object *o = (http_querystring_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	efree(o);
}

#define http_querystring_update(qa, qs) _http_querystring_update((qa), (qs) TSRMLS_CC)
static inline void _http_querystring_update(zval *qarray, zval *qstring TSRMLS_DC)
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

#define http_querystring_modify_array(a, k, l, v) _http_querystring_modify_array((a), (k), (l), (v) TSRMLS_CC)
static inline int _http_querystring_modify_array(zval *qarray, char *key, uint keylen, zval *data TSRMLS_DC)
{
	if (Z_TYPE_P(data) == IS_NULL) {
		if (SUCCESS != zend_hash_del(Z_ARRVAL_P(qarray), key, keylen + 1)) {
			return 0;
		}
	} else {
		ZVAL_ADDREF(data);
		add_assoc_zval(qarray, key, data);
	}
	return 1;
}

#define http_querystring_instantiate(g) _http_querystring_instantiate((g) TSRMLS_CC)
static inline zval *_http_querystring_instantiate(zend_bool global TSRMLS_DC)
{
	zval *zobj, *zglobal;
	
	MAKE_STD_ZVAL(zglobal);
	ZVAL_BOOL(zglobal, global);
	
	MAKE_STD_ZVAL(zobj);
	Z_TYPE_P(zobj) = IS_OBJECT;
	Z_OBJVAL_P(zobj) = http_querystring_object_new(http_querystring_object_ce);
	zend_call_method_with_1_params(&zobj, Z_OBJCE_P(zobj), NULL, "__construct", NULL, zglobal);
	
	zval_ptr_dtor(&zglobal);
	
	return zobj;
}

/* {{{ proto void HttpQueryString::__construct([bool global = true])
 *
 * Creates a new HttpQueryString object instance.
 * Operates on and modifies $_GET and $_SERVER['QUERY_STRING'] if global is TRUE.
 */
PHP_METHOD(HttpQueryString, __construct)
{
	zend_bool global = 1;
	zval *qarray = NULL, *qstring = NULL, **_GET, **_SERVER, **QUERY_STRING;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &global)) {
		if (global) {
			if (	(SUCCESS == zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &_SERVER)) &&
					(Z_TYPE_PP(_SERVER) == IS_ARRAY) &&
					(SUCCESS == zend_hash_find(Z_ARRVAL_PP(_SERVER), "QUERY_STRING", sizeof("QUERY_STRING"), (void **) &QUERY_STRING))) {
				
				qstring = *QUERY_STRING;
				
				if ((SUCCESS == zend_hash_find(&EG(symbol_table), "_GET", sizeof("_GET"), (void **) &_GET)) && (Z_TYPE_PP(_GET) == IS_ARRAY)) {
					qarray = *_GET;
				} else {
					http_error(HE_WARNING, HTTP_E_QUERYSTRING, "Could not acquire reference to superglobal GET array");
				}
			} else {
				http_error(HE_WARNING, HTTP_E_QUERYSTRING, "Could not acquire reference to QUERY_STRING");
			}
			
			if (qarray && qstring) {
				if (Z_TYPE_P(qstring) != IS_STRING) {
					convert_to_string(qstring);
				}
				
				SET_PROP(queryArray, qarray);
				SET_PROP(queryString, qstring);
				GET_PROP(queryArray)->is_ref = 1;
				GET_PROP(queryString)->is_ref = 1;
			}
		} else {
			qarray = ecalloc(1, sizeof(zval));
			array_init(qarray);
			SET_PROP(queryArray, qarray);
			UPD_STRL(queryString, "", 0);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpQueryString::__toString()
 *
 * Returns the string representation.
 */
PHP_METHOD(HttpQueryString, __toString)
{
	NO_ARGS;
	RETURN_PROP(queryString);
}
/* }}} */

/* {{{ proto mixed HttpQueryString::get([string key[, mixed type = 0[, mixed defval = NULL[, bool delete = false]]]])
 *
 * Get (part of) the query string.
 *
 * The type parameter is either one of the HttpQueryString::TYPE_* constants or a type abbreviation like
 * "b" for bool, "i" for int, "f" for float, "s" for string, "a" for array and "o" for a stdClass object.
 */
PHP_METHOD(HttpQueryString, get)
{
	char *name = NULL;
	int name_len = 0;
	long type = 0;
	zend_bool del = 0;
	zval *ztype = NULL, *defval = NULL;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|szzb", &name, &name_len, &ztype, &defval, &del)) {
		if (name && name_len) {
			zval **arrval, *qarray = GET_PROP(queryArray);
			
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(qarray), name, name_len + 1, (void **) &arrval)) {
				RETVAL_ZVAL(*arrval, 1, 0);
				
				if (ztype) {
					if (Z_TYPE_P(ztype) == IS_LONG) {
						type = Z_LVAL_P(ztype);
					} else if(Z_TYPE_P(ztype) == IS_STRING) {
						switch (tolower(Z_STRVAL_P(ztype)[0]))
						{
							case 'b':
								type = HTTP_QUERYSTRING_TYPE_BOOL;
							break;
							case 'i':
								type = HTTP_QUERYSTRING_TYPE_INT;
							break;
							case 'f':
								type = HTTP_QUERYSTRING_TYPE_FLOAT;
							break;	
							case 's':
								type = HTTP_QUERYSTRING_TYPE_STRING;
							break;
							case 'a':
								type = HTTP_QUERYSTRING_TYPE_ARRAY;
							break;
							case 'o':
								type = HTTP_QUERYSTRING_TYPE_OBJECT;
							break;
						}
					}
					if (type) {
						convert_to_type(type, return_value);
					}
				}
				
				if (del && (SUCCESS == zend_hash_del(Z_ARRVAL_P(qarray), name, name_len + 1))) {
					http_querystring_update(qarray, GET_PROP(queryString));
				}
			} else if(defval) {
				RETURN_ZVAL(defval, 1, 0);
			}
		} else {
			RETURN_PROP(queryString);
		}
	}
}
/* }}} */

/* {{{ proto string HttpQueryString::set(string name, mixed value)
 *
 * Set a query string entry.
 */
PHP_METHOD(HttpQueryString, set)
{
	char *name;
	int name_len;
	zval *value;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &value)) {
		zval *qarray = GET_PROP(queryArray);
		
		if (http_querystring_modify_array(qarray, name, name_len, value)) {
			http_querystring_update(qarray, GET_PROP(queryString));
		}
	}
	
	IF_RETVAL_USED {
		RETURN_PROP(queryString);
	}
}
/* }}} */

/* {{{ proto string HttpQueryString::del(mixed param)
 *
 * Deletes entry/entries from the query string.
 */
PHP_METHOD(HttpQueryString, del)
{
	zval *params;
		
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
		zval *qarray = GET_PROP(queryArray);
		
		if (Z_TYPE_P(params) == IS_ARRAY) {
			HashPosition pos;
			zval **name;
			
			FOREACH_VAL(pos, params, name) {
				ZVAL_ADDREF(*name);
				convert_to_string_ex(name);
				zend_hash_del(Z_ARRVAL_P(qarray), Z_STRVAL_PP(name), Z_STRLEN_PP(name) + 1);
				zval_ptr_dtor(name);
			}
			
			http_querystring_update(qarray, GET_PROP(queryString));
		} else {
			ZVAL_ADDREF(params);
			convert_to_string_ex(&params);
			if (SUCCESS == zend_hash_del(Z_ARRVAL_P(qarray), Z_STRVAL_P(params), Z_STRLEN_P(params) + 1)) {
				http_querystring_update(qarray, GET_PROP(queryString));
			}
			zval_ptr_dtor(&params);
		}
	}
	IF_RETVAL_USED {
		RETURN_PROP(queryString);
	}
}
/* }}} */

/* {{{ proto string HttpQueryString::mod(array params)
 *
 * Modifies the query string according to params. NULL values will unset the variable.
 */
PHP_METHOD(HttpQueryString, mod)
{
	zval *params;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &params)) {
		zval **value, *qarray = GET_PROP(queryArray);
		HashPosition pos;
		char *key = NULL;
		uint keylen = 0;
		ulong idx = 0;
		
		FOREACH_KEYLENVAL(pos, params, key, keylen, idx, value) {
			if (key) {
				http_querystring_modify_array(qarray, key, keylen, *value);
			} else {
				keylen = spprintf(&key, 0, "%lu", idx);
				http_querystring_modify_array(qarray, key, keylen, *value);
				efree(key);
			}
			key = NULL;
		}
		
		http_querystring_update(qarray, GET_PROP(queryString));
	}
	
	IF_RETVAL_USED {
		RETURN_PROP(queryString);
	}
}
/* }}} */

/* {{{ proto HttpQueryString HttpQueryString::getInstance([bool global = true])
 *
 * Get a single instance (differentiates between the global setting).
 */
PHP_METHOD(HttpQueryString, getInstance)
{
	zend_bool global = 1;
	zval *instance = GET_STATIC_PROP(instance);
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &global)) {
		zval **zobj_ptr = NULL, *zobj = NULL;
		
		if (Z_TYPE_P(instance) == IS_ARRAY) {
			if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(instance), global, (void **) &zobj_ptr)) {
				RETVAL_ZVAL(*zobj_ptr, 1, 0);
			} else {
				zobj = http_querystring_instantiate(global);
				add_index_zval(instance, global, zobj);
				RETVAL_OBJECT(zobj, 1);
			}
		} else {
			MAKE_STD_ZVAL(instance);
			array_init(instance);
			
			zobj = http_querystring_instantiate(global);
			add_index_zval(instance, global, zobj);
			RETVAL_OBJECT(zobj, 1);
			
			SET_STATIC_PROP(instance, instance);
			zval_ptr_dtor(&instance);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

