/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#include "php_http.h"

#ifdef ZEND_ENGINE_2

#include "php_variables.h"
#include "zend_interfaces.h"

#include "php_http_api.h"
#include "php_http_querystring_api.h"
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
HTTP_BEGIN_ARGS(singleton, 0)
	HTTP_ARG_VAL(global, 0)
HTTP_END_ARGS;
#endif

HTTP_BEGIN_ARGS(factory, 0)
	HTTP_ARG_VAL(global, 0)
	HTTP_ARG_VAL(params, 0)
	HTTP_ARG_VAL(class_name, 0)
HTTP_END_ARGS;

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

HTTP_BEGIN_ARGS(mod, 0)
	HTTP_ARG_VAL(params, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(__getter, 1)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(defval, 0)
	HTTP_ARG_VAL(delete, 0)
HTTP_END_ARGS;

#ifdef HTTP_HAVE_ICONV
HTTP_BEGIN_ARGS(xlate, 2)
	HTTP_ARG_VAL(from_encoding, 0)
	HTTP_ARG_VAL(to_encoding, 0)
HTTP_END_ARGS;
#endif

HTTP_EMPTY_ARGS(serialize);
HTTP_BEGIN_ARGS(unserialize, 1)
	HTTP_ARG_VAL(serialized, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(offsetGet, 1)
	HTTP_ARG_VAL(offset, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(offsetSet, 2)
	HTTP_ARG_VAL(offset, 0)
	HTTP_ARG_VAL(value, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(offsetExists, 1)
	HTTP_ARG_VAL(offset, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(offsetUnset, 1)
	HTTP_ARG_VAL(offset, 0)
HTTP_END_ARGS;


#define OBJ_PROP_CE http_querystring_object_ce
zend_class_entry *http_querystring_object_ce;
zend_function_entry http_querystring_object_fe[] = {
	HTTP_QUERYSTRING_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	
	HTTP_QUERYSTRING_ME(toArray, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(toString, ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpQueryString, __toString, toString, HTTP_ARGS(HttpQueryString, toString), ZEND_ACC_PUBLIC)
	
	HTTP_QUERYSTRING_ME(get, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(set, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(mod, ZEND_ACC_PUBLIC)
	
	HTTP_QUERYSTRING_GME(getBool, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getInt, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getFloat, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getString, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getArray, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_GME(getObject, ZEND_ACC_PUBLIC)
	
	HTTP_QUERYSTRING_ME(factory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#ifndef WONKY
	HTTP_QUERYSTRING_ME(singleton, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#endif
#ifdef HTTP_HAVE_ICONV
	HTTP_QUERYSTRING_ME(xlate, ZEND_ACC_PUBLIC)
#endif
	
	/* Implements Serializable */
	HTTP_QUERYSTRING_ME(serialize, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(unserialize, ZEND_ACC_PUBLIC)
	
	/* Implements ArrayAccess */
	HTTP_QUERYSTRING_ME(offsetGet, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(offsetSet, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(offsetExists, ZEND_ACC_PUBLIC)
	HTTP_QUERYSTRING_ME(offsetUnset, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_querystring_object_handlers;

PHP_MINIT_FUNCTION(http_querystring_object)
{
	HTTP_REGISTER_CLASS_EX(HttpQueryString, http_querystring_object, NULL, 0);
	
#ifndef WONKY
	zend_class_implements(http_querystring_object_ce TSRMLS_CC, 2, zend_ce_serializable, zend_ce_arrayaccess);
#endif
	
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
	return http_querystring_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_querystring_object_new_ex(zend_class_entry *ce, void *nothing, http_querystring_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_querystring_object *o;

	o = ecalloc(1, sizeof(http_querystring_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), zend_hash_num_elements(&ce->default_properties), NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_querystring_object, o);
	ov.handlers = &http_querystring_object_handlers;

	return ov;
}

void _http_querystring_object_free(zend_object *object TSRMLS_DC)
{
	http_querystring_object *o = (http_querystring_object *) object;

	freeObject(o);
}

/* {{{ querystring helpers */
#define http_querystring_instantiate(t, g, p, u) _http_querystring_instantiate((t), (g), (p), (u) TSRMLS_CC)
static inline zval *_http_querystring_instantiate(zval *this_ptr, zend_bool global, zval *params, zend_bool defer_update TSRMLS_DC)
{
	zval *qarray = NULL, *qstring = NULL, **_SERVER = NULL, **_GET = NULL, **QUERY_STRING = NULL;;
	
	if (!this_ptr) {
		MAKE_STD_ZVAL(this_ptr);
		Z_TYPE_P(this_ptr) = IS_OBJECT;
		this_ptr->value.obj = http_querystring_object_new(http_querystring_object_ce);
	}
	if (global) {
#ifdef ZEND_ENGINE_2
		zend_is_auto_global("_SERVER", lenof("_SERVER") TSRMLS_CC);
#endif
		if (	(SUCCESS == zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void *) &_SERVER)) &&
				(Z_TYPE_PP(_SERVER) == IS_ARRAY) &&
				(SUCCESS == zend_hash_find(Z_ARRVAL_PP(_SERVER), "QUERY_STRING", sizeof("QUERY_STRING"), (void *) &QUERY_STRING))) {
			
			qstring = *QUERY_STRING;
#ifdef ZEND_ENGINE_2
			zend_is_auto_global("_GET", lenof("_GET") TSRMLS_CC);
#endif
			if ((SUCCESS == zend_hash_find(&EG(symbol_table), "_GET", sizeof("_GET"), (void *) &_GET)) && (Z_TYPE_PP(_GET) == IS_ARRAY)) {
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
	
			if (params) {
				http_querystring_modify(GET_PROP(queryArray), params);
			}
			if (!defer_update) {
				http_querystring_update(GET_PROP(queryArray), GET_PROP(queryString));
			}
		}
	} else {
		qarray = ecalloc(1, sizeof(zval));
		array_init(qarray);
		
		SET_PROP(queryArray, qarray);
		UPD_STRL(queryString, "", 0);
		
		if (params && http_querystring_modify(qarray, params) && !defer_update) {
			http_querystring_update(qarray, GET_PROP(queryString));
		}
	}
	
	return this_ptr;
}

#define http_querystring_get(o, t, n, l, def, del, r) _http_querystring_get((o), (t), (n), (l), (def), (del), (r) TSRMLS_CC)
static inline void _http_querystring_get(zval *this_ptr, int type, char *name, uint name_len, zval *defval, zend_bool del, zval *return_value TSRMLS_DC)
{
	zval **arrval, *qarray = GET_PROP(queryArray);
		
	if ((Z_TYPE_P(qarray) == IS_ARRAY) && (SUCCESS == zend_hash_find(Z_ARRVAL_P(qarray), name, name_len + 1, (void *) &arrval))) {
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
/* }}} */

/* {{{ proto final void HttpQueryString::__construct([bool global = true[, mixed add])
	Creates a new HttpQueryString object instance. Operates on and modifies $_GET and $_SERVER['QUERY_STRING'] if global is TRUE. */
PHP_METHOD(HttpQueryString, __construct)
{
	zend_bool global = 1;
	zval *params = NULL;
	
	SET_EH_THROW_HTTP();
	if (!sapi_module.treat_data) {
		http_error(HE_ERROR, HTTP_E_QUERYSTRING, "The SAPI does not have a treat_data function registered");
	} else if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bz", &global, &params)) {
		http_querystring_instantiate(getThis(), global, params, 0);
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto HttpQueryString HttpQueryString::factory([bool global = TRUE[, mixed params[, string class_name = "HttpQueryString"])
	Creates a new HttpQueryString object instance. */
PHP_METHOD(HttpQueryString, factory)
{
	zend_bool global = 1;
	zval *params = NULL;
	char *cn = NULL;
	int cl = 0;
	zend_object_value ov;
	
	SET_EH_THROW_HTTP();
	if (!sapi_module.treat_data) {
		http_error(HE_ERROR, HTTP_E_QUERYSTRING, "The SAPI does not have a treat_data function registered");
	} else if (	SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bzs", &global, &params, &cn, &cl) &&
				SUCCESS == http_object_new(&ov, cn, cl, _http_querystring_object_new_ex, http_querystring_object_ce, NULL, NULL)) {
		RETVAL_OBJVAL(ov, 0);
		http_querystring_instantiate(return_value, global, params, 0);
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpQueryString::toString()
	Returns the string representation. */
PHP_METHOD(HttpQueryString, toString)
{
	NO_ARGS;
	RETURN_PROP(queryString);
}
/* }}} */

/* {{{ proto array HttpQueryString::toArray()
	Returns the array representation. */
PHP_METHOD(HttpQueryString, toArray)
{
	NO_ARGS;
	RETURN_PROP(queryArray);
}
/* }}} */

/* {{{ proto mixed HttpQueryString::get([string key[, mixed type = 0[, mixed defval = NULL[, bool delete = false]]]])
	Get (part of) the query string. The type parameter is either one of the HttpQueryString::TYPE_* constants or a type abbreviation like "b" for bool, "i" for int, "f" for float, "s" for string, "a" for array and "o" for a stdClass object. */
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
					switch (Z_STRVAL_P(ztype)[0]) {
						case 'B': 
						case 'b':	type = HTTP_QUERYSTRING_TYPE_BOOL;		break;
						case 'I':
						case 'i':	type = HTTP_QUERYSTRING_TYPE_INT;		break;
						case 'F':
						case 'f':	type = HTTP_QUERYSTRING_TYPE_FLOAT;		break;	
						case 'S':
						case 's':	type = HTTP_QUERYSTRING_TYPE_STRING;	break;
						case 'A':
						case 'a':	type = HTTP_QUERYSTRING_TYPE_ARRAY;		break;
						case 'O':
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
	Set query string entry/entries. NULL values will unset the variable. */
PHP_METHOD(HttpQueryString, set)
{
	zval *params;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
		zval *qarray = GET_PROP(queryArray);
		if (http_querystring_modify(qarray, params)) {
			http_querystring_update(qarray, GET_PROP(queryString));
		}
	}
	
	if (return_value_used) {
		RETURN_PROP(queryString);
	}
}
/* }}} */

/* {{{ proto HttpQueryString HttpQueryString::mod(mixed params)
	Copies the query string object and sets provided params at the clone. */
PHP_METHOD(HttpQueryString, mod)
{
	zval *zobj, *qarr, *qstr, *params;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &params)) {
		zobj = http_querystring_instantiate(NULL, 0, GET_PROP(queryArray), 1);
		qarr = GET_PROP_EX(zobj, queryArray);
		qstr = GET_PROP_EX(zobj, queryString);
		
		http_querystring_modify(qarr, params);
		http_querystring_update(qarr, qstr);
		
		RETURN_ZVAL(zobj, 1, 1);
	}
}
/* }}} */

#ifndef WONKY
/* {{{ proto static HttpQueryString HttpQueryString::singleton([bool global = true])
	Get a single instance (differentiates between the global setting). */
PHP_METHOD(HttpQueryString, singleton)
{
	zend_bool global = 1;
	zval *instance = GET_STATIC_PROP(instance);
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &global)) {
		zval **zobj_ptr = NULL, *zobj = NULL;
		
		if (Z_TYPE_P(instance) == IS_ARRAY) {
			if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(instance), global, (void *) &zobj_ptr)) {
				RETVAL_ZVAL(*zobj_ptr, 1, 0);
			} else {
				zobj = http_querystring_instantiate(NULL, global, NULL, (zend_bool) !global);
				add_index_zval(instance, global, zobj);
				RETVAL_OBJECT(zobj, 1);
			}
		} else {
			MAKE_STD_ZVAL(instance);
			array_init(instance);
			
			zobj = http_querystring_instantiate(NULL, global, NULL, (zend_bool) !global);
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

#ifdef HTTP_HAVE_ICONV
/* {{{ proto bool HttpQueryString::xlate(string ie, string oe)
	Converts the query string from the source encoding ie to the target encoding oe. WARNING: Don't use any character set that can contain NUL bytes like UTF-16. */
PHP_METHOD(HttpQueryString, xlate)
{
	char *ie, *oe;
	int ie_len, oe_len;
	zval xa, *qa, *qs;
	STATUS rs;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &ie, &ie_len, &oe, &oe_len)) {
		RETURN_FALSE;
	}
	
	qa = GET_PROP(queryArray);
	qs = GET_PROP(queryString);
	INIT_PZVAL(&xa);
	array_init(&xa);
	
	if (SUCCESS == (rs = http_querystring_xlate(&xa, qa, ie, oe))) {
		zend_hash_clean(Z_ARRVAL_P(qa));
		zend_hash_copy(Z_ARRVAL_P(qa), Z_ARRVAL(xa), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		http_querystring_update(qa, qs);
	}
	zval_dtor(&xa);
	
	RETURN_SUCCESS(rs);
}
/* }}} */
#endif /* HAVE_ICONV */

/* {{{ proto string HttpQueryString::serialize()
	Implements Serializable::serialize(). */
PHP_METHOD(HttpQueryString, serialize)
{
	NO_ARGS;
	RETURN_PROP(queryString);
}
/* }}} */

/* {{{ proto void HttpQueryString::unserialize(string serialized)
	Implements Serializable::unserialize(). */
PHP_METHOD(HttpQueryString, unserialize)
{
	zval *serialized;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &serialized)) {
		if (Z_TYPE_P(serialized) == IS_STRING) {
			http_querystring_instantiate(getThis(), 0, serialized, 0);
		} else {
			http_error(HE_WARNING, HTTP_E_QUERYSTRING, "Expected a string as parameter");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto mixed HttpQueryString::offsetGet(string offset)
	Implements ArrayAccess::offsetGet(). */
PHP_METHOD(HttpQueryString, offsetGet)
{
	char *offset_str;
	int offset_len;
	zval **value;
	
	if (	(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len)) &&
			(SUCCESS == zend_hash_find(Z_ARRVAL_P(GET_PROP(queryArray)), offset_str, offset_len + 1, (void *) &value))) {
		RETVAL_ZVAL(*value, 1, 0);
	}
}
/* }}} */

/* {{{ proto void HttpQueryString::offsetSet(string offset, mixed value)
	Implements ArrayAccess::offsetGet(). */
PHP_METHOD(HttpQueryString, offsetSet)
{
	char *offset_str;
	int offset_len;
	zval *value;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &offset_str, &offset_len, &value)) {
		zval *qarr = GET_PROP(queryArray), *qstr = GET_PROP(queryString);
		
		ZVAL_ADDREF(value);
		add_assoc_zval_ex(qarr, offset_str, offset_len + 1, value);
		http_querystring_update(qarr, qstr);
	}
}
/* }}} */

/* {{{ proto bool HttpQueryString::offsetExists(string offset)
	Implements ArrayAccess::offsetExists(). */
PHP_METHOD(HttpQueryString, offsetExists)
{
	char *offset_str;
	int offset_len;
	zval **value;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len)) {
		RETURN_BOOL((SUCCESS == zend_hash_find(Z_ARRVAL_P(GET_PROP(queryArray)), offset_str, offset_len + 1, (void *) &value)) && (Z_TYPE_PP(value) != IS_NULL));
	}
}
/* }}} */

/* {{{ proto void HttpQueryString::offsetUnset(string offset)
	Implements ArrayAccess::offsetUnset(). */
PHP_METHOD(HttpQueryString, offsetUnset)
{
	char *offset_str;
	int offset_len;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &offset_str, &offset_len)) {
		zval *qarr = GET_PROP(queryArray);
		
		if (SUCCESS == zend_hash_del(Z_ARRVAL_P(qarr), offset_str, offset_len + 1)) {
			http_querystring_update(qarr, GET_PROP(queryString));
		}
	}
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

