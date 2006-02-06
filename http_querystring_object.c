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

#ifdef ZEND_ENGINE_2

#include "php_variables.h"
#include "zend_interfaces.h"

#include "php_http_api.h"
#include "php_http_url_api.h"
#include "php_http_querystring_object.h"
#include "php_http_exception_object.h"

#define HTTP_BEGIN_ARGS(method, req_args) 			HTTP_BEGIN_ARGS_EX(HttpQueryString, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)						HTTP_EMPTY_ARGS_EX(HttpQueryString, method, 0)
#define HTTP_QUERYSTRING_ME(method, visibility)		PHP_ME(HttpQueryString, method, HTTP_ARGS(HttpQueryString, method), visibility)
#define HTTP_QUERYSTRING_GME(method, visibility)	PHP_ME(HttpQueryString, method, HTTP_ARGS(HttpQueryString, __getter), visibility)

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(global, 0)
	HTTP_ARG_VAL(params, 0)
HTTP_END_ARGS;

#ifndef WONKY
HTTP_BEGIN_ARGS(getInstance, 0)
	HTTP_ARG_VAL(global, 0)
HTTP_END_ARGS;
#endif

HTTP_EMPTY_ARGS(toArray);
HTTP_EMPTY_ARGS(toString);

HTTP_BEGIN_ARGS(get, 0)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(type, 0)
	HTTP_ARG_VAL(defval, 0)
	HTTP_ARG_VAL(delete, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(set, 1)
	HTTP_ARG_VAL(params, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(__getter, 1)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(defval, 0)
	HTTP_ARG_VAL(delete, 0)
HTTP_END_ARGS;

#define http_querystring_object_declare_default_properties() _http_querystring_object_declare_default_properties(TSRMLS_C)
static inline void _http_querystring_object_declare_default_properties(TSRMLS_D);

#define GET_STATIC_PROP(n) *GET_STATIC_PROP_EX(http_querystring_object_ce, n)
#define SET_STATIC_PROP(n, v) SET_STATIC_PROP_EX(http_querystring_object_ce, n, v)
#define OBJ_PROP_CE http_querystring_object_ce
zend_class_entry *http_querystring_object_ce;
zend_function_entry http_querystring_object_fe[] = {
	HTTP_QUERYSTRING_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	
	HTTP_QUERYSTRING_ME(toArray, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpQueryString, __toString, toString, HTTP_ARGS(HttpQueryString, toString), ZEND_ACC_PUBLIC)
	
	HTTP_QUERYSTRING_ME(get, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(set, ZEND_ACC_PUBLIC)
	
#ifndef WONKY
	HTTP_QUERYSTRING_ME(getInstance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#endif
	
	HTTP_QUERYSTRING_GME(getBool, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getInt, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getFloat, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getString, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getArray, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getObject, ZEND_ACC_PUBLIC)
	
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

#define http_querystring_modify_ex(a, k, l, v) _http_querystring_modify_ex((a), (k), (l), (v) TSRMLS_CC)
static inline int _http_querystring_modify_ex(zval *qarray, char *key, uint keylen, zval *data TSRMLS_DC)
{
	if (Z_TYPE_P(data) == IS_NULL) {
		if (SUCCESS != zend_hash_del(Z_ARRVAL_P(qarray), key, keylen)) {
			return 0;
		}
	} else {
		ZVAL_ADDREF(data);
		add_assoc_zval(qarray, key, data);
	}
	return 1;
}

#define http_querystring_modify_array(q, a) _http_querystring_modify_array((q), (a) TSRMLS_CC)
static inline int _http_querystring_modify_array(zval *qarray, zval *array TSRMLS_DC)
{
	zval **value;
	HashPosition pos;
	char *key = NULL;
	uint keylen = 0;
	ulong idx = 0;
	int rv = 0;
		
	FOREACH_KEYLENVAL(pos, array, key, keylen, idx, value) {
		if (key) {
			if (http_querystring_modify_ex(qarray, key, keylen, *value)) {
				rv = 1;
			}
		} else {
			keylen = spprintf(&key, 0, "%lu", idx);
			if (http_querystring_modify_ex(qarray, key, keylen, *value)) {
				rv = 1;
			}
			efree(key);
		}
		key = NULL;
	}
	
	return rv;
}

#define http_querystring_modify(q, p) _http_querystring_modify((q), (p) TSRMLS_CC)
static inline int _http_querystring_modify(zval *qarray, zval *params TSRMLS_DC)
{
	if ((Z_TYPE_P(params) == IS_OBJECT) && instanceof_function(Z_OBJCE_P(params), http_querystring_object_ce TSRMLS_CC)) {
		return http_querystring_modify_array(qarray, GET_PROP_EX(params, queryArray));
	} else if (Z_TYPE_P(params) == IS_ARRAY) {
		return http_querystring_modify_array(qarray, params);
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

#ifndef WONKY
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
#endif

#define http_querystring_get(o, t, n, l, def, del, r) _http_querystring_get((o), (t), (n), (l), (def), (del), (r) TSRMLS_CC)
static inline void _http_querystring_get(zval *this_ptr, int type, char *name, uint name_len, zval *defval, zend_bool del, zval *return_value TSRMLS_DC)
{
	zval **arrval, *qarray = GET_PROP(queryArray);
		
	if ((Z_TYPE_P(qarray) == IS_ARRAY) && (SUCCESS == zend_hash_find(Z_ARRVAL_P(qarray), name, name_len + 1, (void **) &arrval))) {
		RETVAL_ZVAL(*arrval, 1, 0);
		
		if (type) {
			convert_to_type(type, return_value);
		}
			
		if (del && (SUCCESS == zend_hash_del(Z_ARRVAL_P(qarray), name, name_len + 1))) {
			http_querystring_update(qarray, GET_PROP(queryString));
		}
	} else if(defval) {
		RETURN_ZVAL(defval, 1, 0);
	}
}

/* {{{ proto void HttpQueryString::__construct([bool global = true[, mixed add])
 *
 * Creates a new HttpQueryString object instance.
 * Operates on and modifies $_GET and $_SERVER['QUERY_STRING'] if global is TRUE.
 */
PHP_METHOD(HttpQueryString, __construct)
{
	zend_bool global = 1;
	zval *params = NULL, *qarray = NULL, *qstring = NULL, **_GET, **_SERVER, **QUERY_STRING;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bz", &global, &params)) {
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
				
				if (params && http_querystring_modify(GET_PROP(queryArray), params)) {
					http_querystring_update(GET_PROP(queryArray), GET_PROP(queryString));
				}
			}
		} else {
			qarray = ecalloc(1, sizeof(zval));
			array_init(qarray);
			
			SET_PROP(queryArray, qarray);
			UPD_STRL(queryString, "", 0);
			
			if (params && http_querystring_modify(qarray, params)) {
				http_querystring_update(qarray, GET_PROP(queryString));
			}
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpQueryString::toString()
 *
 * Returns the string representation.
 */
PHP_METHOD(HttpQueryString, toString)
{
	NO_ARGS;
	RETURN_PROP(queryString);
}
/* }}} */

/* {{{ proto array HttpQueryString::toArray()
 *
 * Returns the array representation.
 */
PHP_METHOD(HttpQueryString, toArray)
{
	NO_ARGS;
	RETURN_PROP(queryArray);
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
			if (ztype) {
				if (Z_TYPE_P(ztype) == IS_LONG) {
					type = Z_LVAL_P(ztype);
				} else if(Z_TYPE_P(ztype) == IS_STRING) {
					switch (tolower(Z_STRVAL_P(ztype)[0]))
					{
						case 'b':	type = HTTP_QUERYSTRING_TYPE_BOOL;		break;
						case 'i':	type = HTTP_QUERYSTRING_TYPE_INT;		break;
						case 'f':	type = HTTP_QUERYSTRING_TYPE_FLOAT;		break;	
						case 's':	type = HTTP_QUERYSTRING_TYPE_STRING;	break;
						case 'a':	type = HTTP_QUERYSTRING_TYPE_ARRAY;		break;
						case 'o':	type = HTTP_QUERYSTRING_TYPE_OBJECT;	break;
					}
				}
			}
			http_querystring_get(getThis(), type, name, name_len, defval, del, return_value);
		} else {
			RETURN_PROP(queryString);
		}
	}
}
/* }}} */

/* {{{ proto string HttpQueryString::set(mixed params)
 *
 * Set query string entry/entries. NULL values will unset the variable.
 */
PHP_METHOD(HttpQueryString, set)
{
	zval *params;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
		zval *qarray = GET_PROP(queryArray);
		if (http_querystring_modify(qarray, params)) {
			http_querystring_update(qarray, GET_PROP(queryString));
		}
	}
	
	IF_RETVAL_USED {
		RETURN_PROP(queryString);
	}
}
/* }}} */

#ifndef WONKY
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
#endif

/* {{{ Getters by type */
#define HTTP_QUERYSTRING_GETTER(method, TYPE) \
PHP_METHOD(HttpQueryString, method) \
{ \
	char *name; \
	int name_len; \
	zval *defval = NULL; \
	zend_bool del = 0; \
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|zb", &name, &name_len, &defval, &del)) { \
		http_querystring_get(getThis(), TYPE, name, name_len, defval, del, return_value); \
	} \
}
HTTP_QUERYSTRING_GETTER(getBool, IS_BOOL);
HTTP_QUERYSTRING_GETTER(getInt, IS_LONG);
HTTP_QUERYSTRING_GETTER(getFloat, IS_DOUBLE);
HTTP_QUERYSTRING_GETTER(getString, IS_STRING);
HTTP_QUERYSTRING_GETTER(getArray, IS_ARRAY);
HTTP_QUERYSTRING_GETTER(getObject, IS_OBJECT);
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

