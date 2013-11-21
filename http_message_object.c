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

/* $Id$ */

#define HTTP_WANT_SAPI
#define HTTP_WANT_CURL
#define HTTP_WANT_MAGIC
#include "php_http.h"

#ifdef ZEND_ENGINE_2

#include "zend_interfaces.h"
#include "ext/standard/url.h"
#include "php_variables.h"

#include "php_http_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"
#include "php_http_message_api.h"
#include "php_http_message_object.h"
#include "php_http_exception_object.h"
#include "php_http_response_object.h"
#include "php_http_request_method_api.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#include "php_http_headers_api.h"

#if defined(HTTP_HAVE_SPL) && !defined(WONKY)
/* SPL doesn't install its headers */
extern PHPAPI zend_class_entry *spl_ce_Countable;
#endif

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpMessage, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpMessage, method, 0)
#define HTTP_MESSAGE_ME(method, visibility)	PHP_ME(HttpMessage, method, HTTP_ARGS(HttpMessage, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(message, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(factory, 0)
	HTTP_ARG_VAL(message, 0)
	HTTP_ARG_VAL(class_name, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(fromEnv, 1)
	HTTP_ARG_VAL(type, 0)
	HTTP_ARG_VAL(class_name, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getBody);
HTTP_BEGIN_ARGS(setBody, 1)
	HTTP_ARG_VAL(body, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(getHeader, 1)
	HTTP_ARG_VAL(header, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHeaders);
HTTP_BEGIN_ARGS(setHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
	HTTP_ARG_VAL(append, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getType);
HTTP_BEGIN_ARGS(setType, 1)
	HTTP_ARG_VAL(type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getInfo);
HTTP_BEGIN_ARGS(setInfo, 1)
	HTTP_ARG_VAL(http_info, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseCode);
HTTP_BEGIN_ARGS(setResponseCode, 1)
	HTTP_ARG_VAL(response_code, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseStatus);
HTTP_BEGIN_ARGS(setResponseStatus, 1)
	HTTP_ARG_VAL(response_status, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestMethod);
HTTP_BEGIN_ARGS(setRequestMethod, 1)
	HTTP_ARG_VAL(request_method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestUrl);
HTTP_BEGIN_ARGS(setRequestUrl, 1)
	HTTP_ARG_VAL(url, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHttpVersion);
HTTP_BEGIN_ARGS(setHttpVersion, 1)
	HTTP_ARG_VAL(http_version, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(guessContentType, 1)
	HTTP_ARG_VAL(magic_file, 0)
	HTTP_ARG_VAL(magic_mode, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getParentMessage);
HTTP_EMPTY_ARGS(send);
HTTP_EMPTY_ARGS(__toString);
HTTP_BEGIN_ARGS(toString, 0)
	HTTP_ARG_VAL(include_parent, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(toMessageTypeObject);

HTTP_EMPTY_ARGS(count);

HTTP_EMPTY_ARGS(serialize);
HTTP_BEGIN_ARGS(unserialize, 1)
	HTTP_ARG_VAL(serialized, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(rewind);
HTTP_EMPTY_ARGS(valid);
HTTP_EMPTY_ARGS(key);
HTTP_EMPTY_ARGS(current);
HTTP_EMPTY_ARGS(next);

HTTP_EMPTY_ARGS(detach);
HTTP_BEGIN_ARGS(prepend, 1)
	HTTP_ARG_OBJ(HttpMessage, message, 0)
HTTP_END_ARGS;
HTTP_EMPTY_ARGS(reverse);

#define http_message_object_read_prop _http_message_object_read_prop
static zval *_http_message_object_read_prop(zval *object, zval *member, int type ZEND_LITERAL_KEY_DC TSRMLS_DC);
#define http_message_object_write_prop _http_message_object_write_prop
static void _http_message_object_write_prop(zval *object, zval *member, zval *value ZEND_LITERAL_KEY_DC TSRMLS_DC);
#define http_message_object_get_prop_ptr _http_message_object_get_prop_ptr
static zval **_http_message_object_get_prop_ptr(zval *object, zval *member ZEND_GET_PPTR_TYPE_DC ZEND_LITERAL_KEY_DC TSRMLS_DC);
#define http_message_object_get_props _http_message_object_get_props
static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC);

#define THIS_CE http_message_object_ce
zend_class_entry *http_message_object_ce;
zend_function_entry http_message_object_fe[] = {
	HTTP_MESSAGE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_MESSAGE_ME(getBody, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setBody, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHeader, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(addHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getInfo, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setInfo, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setResponseStatus, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestUrl, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestUrl, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(guessContentType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getParentMessage, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(send, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(toString, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(toMessageTypeObject, ZEND_ACC_PUBLIC)

	/* implements Countable */
	HTTP_MESSAGE_ME(count, ZEND_ACC_PUBLIC)
	
	/* implements Serializable */
	HTTP_MESSAGE_ME(serialize, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(unserialize, ZEND_ACC_PUBLIC)
	
	/* implements Iterator */
	HTTP_MESSAGE_ME(rewind, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(valid, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(current, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(key, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(next, ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, HTTP_ARGS(HttpMessage, __toString), ZEND_ACC_PUBLIC)

	HTTP_MESSAGE_ME(factory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_MALIAS(HttpMessage, fromString, factory, HTTP_ARGS(HttpMessage, factory), ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	HTTP_MESSAGE_ME(fromEnv, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	
	HTTP_MESSAGE_ME(detach, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(prepend, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(reverse, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_message_object_handlers;

static HashTable http_message_object_prophandlers;

typedef void (*http_message_object_prophandler_func)(http_message_object *o, zval *v TSRMLS_DC);

typedef struct _http_message_object_prophandler {
	http_message_object_prophandler_func read;
	http_message_object_prophandler_func write;
} http_message_object_prophandler;

static STATUS http_message_object_add_prophandler(const char *prop_str, size_t prop_len, http_message_object_prophandler_func read, http_message_object_prophandler_func write) {
	http_message_object_prophandler h = { read, write };
	return zend_hash_add(&http_message_object_prophandlers, prop_str, prop_len, (void *) &h, sizeof(h), NULL);
}
static STATUS http_message_object_get_prophandler(const char *prop_str, size_t prop_len, http_message_object_prophandler **handler) {
	return zend_hash_find(&http_message_object_prophandlers, prop_str, prop_len, (void *) handler);
}
static void http_message_object_prophandler_get_type(http_message_object *obj, zval *return_value TSRMLS_DC) {
	RETVAL_LONG(obj->message->type);
}
static void http_message_object_prophandler_set_type(http_message_object *obj, zval *value TSRMLS_DC) {
	zval *cpy = http_zsep(IS_LONG, value);
	http_message_set_type(obj->message, Z_LVAL_P(cpy));
	zval_ptr_dtor(&cpy);
}
static void http_message_object_prophandler_get_body(http_message_object *obj, zval *return_value TSRMLS_DC) {
	phpstr_fix(PHPSTR(obj->message));
	RETVAL_PHPSTR(PHPSTR(obj->message), 0, 1);
}
static void http_message_object_prophandler_set_body(http_message_object *obj, zval *value TSRMLS_DC) {
	zval *cpy = http_zsep(IS_STRING, value);
	phpstr_dtor(PHPSTR(obj->message));
	phpstr_from_string_ex(PHPSTR(obj->message), Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
	zval_ptr_dtor(&cpy);
}
static void http_message_object_prophandler_get_request_method(http_message_object *obj, zval *return_value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(REQUEST, obj->message) && obj->message->http.info.request.method) {
		RETVAL_STRING(obj->message->http.info.request.method, 1);
	} else {
		RETVAL_NULL();
	}
}
static void http_message_object_prophandler_set_request_method(http_message_object *obj, zval *value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(REQUEST, obj->message)) {
		zval *cpy = http_zsep(IS_STRING, value);
		STR_SET(obj->message->http.info.request.method, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void http_message_object_prophandler_get_request_url(http_message_object *obj, zval *return_value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(REQUEST, obj->message) && obj->message->http.info.request.url) {
		RETVAL_STRING(obj->message->http.info.request.url, 1);
	} else {
		RETVAL_NULL();
	}
}
static void http_message_object_prophandler_set_request_url(http_message_object *obj, zval *value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(REQUEST, obj->message)) {
		zval *cpy = http_zsep(IS_STRING, value);
		STR_SET(obj->message->http.info.request.url, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void http_message_object_prophandler_get_response_status(http_message_object *obj, zval *return_value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(RESPONSE, obj->message) && obj->message->http.info.response.status) {
		RETVAL_STRING(obj->message->http.info.response.status, 1);
	} else {
		RETVAL_NULL();
	}
}
static void http_message_object_prophandler_set_response_status(http_message_object *obj, zval *value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		zval *cpy = http_zsep(IS_STRING, value);
		STR_SET(obj->message->http.info.response.status, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void http_message_object_prophandler_get_response_code(http_message_object *obj, zval *return_value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		RETVAL_LONG(obj->message->http.info.response.code);
	} else {
		RETVAL_NULL();
	}
}
static void http_message_object_prophandler_set_response_code(http_message_object *obj, zval *value TSRMLS_DC) {
	if (HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		zval *cpy = http_zsep(IS_LONG, value);
		obj->message->http.info.response.code = Z_LVAL_P(cpy);
		zval_ptr_dtor(&cpy);
	}
}
static void http_message_object_prophandler_get_http_version(http_message_object *obj, zval *return_value TSRMLS_DC) {
	RETVAL_DOUBLE(obj->message->http.version);
}
static void http_message_object_prophandler_set_http_version(http_message_object *obj, zval *value TSRMLS_DC) {
	zval *cpy = http_zsep(IS_DOUBLE, value);
	obj->message->http.version = Z_DVAL_P(cpy);
	zval_ptr_dtor(&cpy);
}
static void http_message_object_prophandler_get_headers(http_message_object *obj, zval *return_value TSRMLS_DC) {
	array_init(return_value);
	zend_hash_copy(Z_ARRVAL_P(return_value), &obj->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
}
static void http_message_object_prophandler_set_headers(http_message_object *obj, zval *value TSRMLS_DC) {
	zval *cpy = http_zsep(IS_ARRAY, value);
	zend_hash_clean(&obj->message->hdrs);
	zend_hash_copy(&obj->message->hdrs, Z_ARRVAL_P(cpy), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	zval_ptr_dtor(&cpy);
}
static void http_message_object_prophandler_get_parent_message(http_message_object *obj, zval *return_value TSRMLS_DC) {
	if (obj->message->parent) {
		RETVAL_OBJVAL(obj->parent, 1);
	} else {
		RETVAL_NULL();
	}
}
static void http_message_object_prophandler_set_parent_message(http_message_object *obj, zval *value TSRMLS_DC) {
	if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), http_message_object_ce TSRMLS_CC)) {
		if (obj->message->parent) {
			zval tmp;
			tmp.value.obj = obj->parent;
			Z_OBJ_DELREF(tmp);
		}
		Z_OBJ_ADDREF_P(value);
		obj->parent = value->value.obj;
	}
}

PHP_MINIT_FUNCTION(http_message_object)
{
	HTTP_REGISTER_CLASS_EX(HttpMessage, http_message_object, NULL, 0);
	
#ifndef WONKY
#	ifdef HTTP_HAVE_SPL
	zend_class_implements(http_message_object_ce TSRMLS_CC, 3, spl_ce_Countable, zend_ce_serializable, zend_ce_iterator);
#	else
	zend_class_implements(http_message_object_ce TSRMLS_CC, 2, zend_ce_serializable, zend_ce_iterator);
#	endif
#else
	zend_class_implements(http_message_object_ce TSRMLS_CC, 1, zend_ce_iterator);
#endif
	
	http_message_object_handlers.clone_obj = _http_message_object_clone_obj;
	http_message_object_handlers.read_property = http_message_object_read_prop;
	http_message_object_handlers.write_property = http_message_object_write_prop;
	http_message_object_handlers.get_properties = http_message_object_get_props;
	http_message_object_handlers.get_property_ptr_ptr = http_message_object_get_prop_ptr;

	zend_hash_init(&http_message_object_prophandlers, 9, NULL, NULL, 1);
	zend_declare_property_long(THIS_CE, ZEND_STRS("type")-1, HTTP_MSG_NONE, ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("type")-1, http_message_object_prophandler_get_type, http_message_object_prophandler_set_type);
	zend_declare_property_string(THIS_CE, ZEND_STRS("body")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("body")-1, http_message_object_prophandler_get_body, http_message_object_prophandler_set_body);
	zend_declare_property_string(THIS_CE, ZEND_STRS("requestMethod")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("requestMethod")-1, http_message_object_prophandler_get_request_method, http_message_object_prophandler_set_request_method);
	zend_declare_property_string(THIS_CE, ZEND_STRS("requestUrl")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("requestUrl")-1, http_message_object_prophandler_get_request_url, http_message_object_prophandler_set_request_url);
	zend_declare_property_string(THIS_CE, ZEND_STRS("responseStatus")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("responseStatus")-1, http_message_object_prophandler_get_response_status, http_message_object_prophandler_set_response_status);
	zend_declare_property_long(THIS_CE, ZEND_STRS("responseCode")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("responseCode")-1, http_message_object_prophandler_get_response_code, http_message_object_prophandler_set_response_code);
	zend_declare_property_null(THIS_CE, ZEND_STRS("httpVersion")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("httpVersion")-1, http_message_object_prophandler_get_http_version, http_message_object_prophandler_set_http_version);
	zend_declare_property_null(THIS_CE, ZEND_STRS("headers")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("headers")-1, http_message_object_prophandler_get_headers, http_message_object_prophandler_set_headers);
	zend_declare_property_null(THIS_CE, ZEND_STRS("parentMessage")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	http_message_object_add_prophandler(ZEND_STRS("parentMessage")-1, http_message_object_prophandler_get_parent_message, http_message_object_prophandler_set_parent_message);
	
#ifndef WONKY
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("TYPE_NONE")-1, HTTP_MSG_NONE TSRMLS_CC);
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("TYPE_REQUEST")-1, HTTP_MSG_REQUEST TSRMLS_CC);
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("TYPE_RESPONSE")-1, HTTP_MSG_RESPONSE TSRMLS_CC);
#endif
	
	HTTP_LONG_CONSTANT("HTTP_MSG_NONE", HTTP_MSG_NONE);
	HTTP_LONG_CONSTANT("HTTP_MSG_REQUEST", HTTP_MSG_REQUEST);
	HTTP_LONG_CONSTANT("HTTP_MSG_RESPONSE", HTTP_MSG_RESPONSE);
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_message_object)
{
	zend_hash_destroy(&http_message_object_prophandlers);

	return SUCCESS;
}

void _http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC)
{
	int i;
	getObject(http_message_object, obj);
	
	/* count */
	http_message_count(i, obj->message);
	
	if (i > 1) {
		zval o;
		zend_object_value *ovalues = NULL;
		http_message_object **objects = NULL;
		int last = i - 1;
		
		objects = ecalloc(i, sizeof(http_message_object *));
		ovalues = ecalloc(i, sizeof(zend_object_value));
	
		/* we are the first message */
		objects[0] = obj;
		ovalues[0] = getThis()->value.obj;
	
		/* fetch parents */
		INIT_PZVAL(&o);
		o.type = IS_OBJECT;
		for (i = 1; obj->parent.handle; ++i) {
			o.value.obj = obj->parent;
			ovalues[i] = o.value.obj;
			objects[i] = obj = zend_object_store_get_object(&o TSRMLS_CC);
		}
		
		/* reorder parents */
		for (last = --i; i; --i) {
			objects[i]->message->parent = objects[i-1]->message;
			objects[i]->parent = ovalues[i-1];
		}
		objects[0]->message->parent = NULL;
		objects[0]->parent.handle = 0;
		objects[0]->parent.handlers = NULL;
		
		/* add ref (why?) */
		Z_OBJ_ADDREF_P(getThis());
		RETVAL_OBJVAL(ovalues[last], 1);
		
		efree(objects);
		efree(ovalues);
	} else {
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

void _http_message_object_prepend_ex(zval *this_ptr, zval *prepend, zend_bool top TSRMLS_DC)
{
	zval m;
	http_message *save_parent_msg = NULL;
	zend_object_value save_parent_obj = {0, NULL};
	getObject(http_message_object, obj);
	getObjectEx(http_message_object, prepend_obj, prepend);
		
	INIT_PZVAL(&m);
	m.type = IS_OBJECT;
		
	if (!top) {
		save_parent_obj = obj->parent;
		save_parent_msg = obj->message->parent;
	} else {
		/* iterate to the most parent object */
		while (obj->parent.handle) {
			m.value.obj = obj->parent;
			obj = zend_object_store_get_object(&m TSRMLS_CC);
		}
	}
		
	/* prepend */
	obj->parent = prepend->value.obj;
	obj->message->parent = prepend_obj->message;
		
	/* add ref */
	zend_objects_store_add_ref(prepend TSRMLS_CC);
	while (prepend_obj->parent.handle) {
		m.value.obj = prepend_obj->parent;
		zend_objects_store_add_ref(&m TSRMLS_CC);
		prepend_obj = zend_object_store_get_object(&m TSRMLS_CC);
	}
		
	if (!top) {
		prepend_obj->parent = save_parent_obj;
		prepend_obj->message->parent = save_parent_msg;
	}
}

zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_message_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_message_object_new_ex(zend_class_entry *ce, http_message *msg, http_message_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_message_object *o;

	o = ecalloc(1, sizeof(http_message_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (msg) {
		o->message = msg;
		if (msg->parent) {
			o->parent = http_message_object_new_ex(ce, msg->parent, NULL);
		}
	}

	
#ifdef ZEND_ENGINE_2_4
	zend_object_std_init(o, ce TSRMLS_CC);
	object_properties_init(o, ce);
#else
	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), zend_hash_num_elements(&ce->default_properties), NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
#endif

	ov.handle = putObject(http_message_object, o);
	ov.handlers = &http_message_object_handlers;

	return ov;
}

zend_object_value _http_message_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	http_message_object *new_obj = NULL;
	getObject(http_message_object, old_obj);
	
	new_ov = http_message_object_new_ex(old_obj->zo.ce, http_message_dup(old_obj->message), &new_obj);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	
	return new_ov;
}

void _http_message_object_free(zend_object *object TSRMLS_DC)
{
	http_message_object *o = (http_message_object *) object;

	if (o->iterator) {
		zval_ptr_dtor(&o->iterator);
		o->iterator = NULL;
	}
	if (o->message) {
		http_message_dtor(o->message);
		efree(o->message);
	}
	if (o->parent.handle) {
		zval p;
		
		INIT_PZVAL(&p);
		p.type = IS_OBJECT;
		p.value.obj = o->parent;
		zend_objects_store_del_ref(&p TSRMLS_CC);
	}
	freeObject(o);
}

static zval **_http_message_object_get_prop_ptr(zval *object, zval *member ZEND_GET_PPTR_TYPE_DC ZEND_LITERAL_KEY_DC TSRMLS_DC) {
	getObjectEx(http_message_object, obj, object);
	http_message_object_prophandler *handler;
	
	if (SUCCESS == http_message_object_get_prophandler(Z_STRVAL_P(member), Z_STRLEN_P(member), &handler)) {
		zend_error(E_ERROR, "Cannot access HttpMessage properties by reference or array key/index");
		return NULL;
	}

	return zend_get_std_object_handlers()->get_property_ptr_ptr(object, member ZEND_GET_PPTR_TYPE_CC ZEND_LITERAL_KEY_CC TSRMLS_CC);
}

static zval *_http_message_object_read_prop(zval *object, zval *member, int type ZEND_LITERAL_KEY_DC TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message_object_prophandler *handler;
	zval *return_value;

	if (SUCCESS == http_message_object_get_prophandler(Z_STRVAL_P(member), Z_STRLEN_P(member), &handler)) {
		if (type == BP_VAR_W) {
			zend_error(E_ERROR, "Cannot access HttpMessage properties by reference or array key/index");
			return NULL;
		}

		ALLOC_ZVAL(return_value);
#ifdef Z_SET_REFCOUNT
		Z_SET_REFCOUNT_P(return_value, 0);
		Z_UNSET_ISREF_P(return_value);
#else
		return_value->refcount = 0;
		return_value->is_ref = 0;
#endif

		handler->read(obj, return_value TSRMLS_CC);

	} else {
		return_value = zend_get_std_object_handlers()->read_property(object, member, type ZEND_LITERAL_KEY_CC TSRMLS_CC);
	}
	
	return return_value;
}

static void _http_message_object_write_prop(zval *object, zval *member, zval *value ZEND_LITERAL_KEY_DC TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message_object_prophandler *handler;
	
	if (SUCCESS == http_message_object_get_prophandler(Z_STRVAL_P(member), Z_STRLEN_P(member), &handler)) {
		handler->write(obj, value TSRMLS_CC);
	} else {
		zend_get_std_object_handlers()->write_property(object, member, value ZEND_LITERAL_KEY_CC TSRMLS_CC);
	}
}

static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC)
{
	zval *headers;
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	zval array, *parent;
#ifdef ZEND_ENGINE_2_4
	HashTable *props = zend_get_std_object_handlers()->get_properties(object TSRMLS_CC);
#else
	HashTable *props = OBJ_PROP(obj);
#endif
	INIT_ZARR(array, props);

#define ASSOC_PROP(array, ptype, name, val) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_ ##ptype## _ex(&array, m_prop_name, sizeof(name)+3, val); \
		efree(m_prop_name); \
	}
#define ASSOC_STRING(array, name, val) ASSOC_STRINGL(array, name, val, strlen(val))
#define ASSOC_STRINGL(array, name, val, len) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_stringl_ex(&array, m_prop_name, sizeof(name)+3, val, len, 1); \
		efree(m_prop_name); \
	}

	ASSOC_PROP(array, long, "type", msg->type);
	ASSOC_PROP(array, double, "httpVersion", msg->http.version);

	switch (msg->type) {
		case HTTP_MSG_REQUEST:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRING(array, "requestMethod", STR_PTR(msg->http.info.request.method));
			ASSOC_STRING(array, "requestUrl", STR_PTR(msg->http.info.request.url));
			break;

		case HTTP_MSG_RESPONSE:
			ASSOC_PROP(array, long, "responseCode", msg->http.info.response.code);
			ASSOC_STRING(array, "responseStatus", STR_PTR(msg->http.info.response.status));
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
			break;

		case HTTP_MSG_NONE:
		default:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
			break;
	}

	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	ASSOC_PROP(array, zval, "headers", headers);
	ASSOC_STRINGL(array, "body", PHPSTR_VAL(msg), PHPSTR_LEN(msg));
	
	MAKE_STD_ZVAL(parent);
	if (msg->parent) {
		ZVAL_OBJVAL(parent, obj->parent, 1);
	} else {
		ZVAL_NULL(parent);
	}
	ASSOC_PROP(array, zval, "parentMessage", parent);

	return props;
}

/* ### USERLAND ### */

/* {{{ proto void HttpMessage::__construct([string message])
	Create a new HttpMessage object instance. */
PHP_METHOD(HttpMessage, __construct)
{
	int length = 0;
	char *message = NULL;
	
	getObject(http_message_object, obj);
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &message, &length) && message && length) {
		http_message *msg = obj->message;
		
		http_message_dtor(msg);
		if ((obj->message = http_message_parse_ex(msg, message, length))) {
			if (obj->message->parent) {
				obj->parent = http_message_object_new_ex(Z_OBJCE_P(getThis()), obj->message->parent, NULL);
			}
		} else {
			obj->message = http_message_init(msg);
		}
	}
	if (!obj->message) {
		obj->message = http_message_new();
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto static HttpMessage HttpMessage::factory([string raw_message[, string class_name = "HttpMessage"]])
	Create a new HttpMessage object instance. */
PHP_METHOD(HttpMessage, factory)
{
	char *string = NULL, *cn = NULL;
	int length = 0, cl = 0;
	http_message *msg = NULL;
	zend_object_value ov;
	http_message_object *obj = NULL;

	RETVAL_NULL();
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &string, &length, &cn, &cl)) {
		if (length) {
			msg = http_message_parse(string, length);
		}
		if ((msg || !length) && SUCCESS == http_object_new(&ov, cn, cl, _http_message_object_new_ex, http_message_object_ce, msg, &obj)) {
			RETVAL_OBJVAL(ov, 0);
		}
		if (obj && !obj->message) {
			obj->message = http_message_new();
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto static HttpMessage HttpMessage::fromEnv(int type[, string class_name = "HttpMessage"])
	Create a new HttpMessage object from environment representing either current request or response */
PHP_METHOD(HttpMessage, fromEnv)
{
	char *cn = NULL;
	int cl = 0;
	long type;
	http_message_object *obj = NULL;
	zend_object_value ov;
	
	RETVAL_NULL();
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|s", &type, &cn, &cl)) {
		if (SUCCESS == http_object_new(&ov, cn, cl, _http_message_object_new_ex, http_message_object_ce, http_message_init_env(NULL, type), &obj)) {
			RETVAL_OBJVAL(ov, 0);
		}
		if (obj && !obj->message) {
			obj->message = http_message_new();
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpMessage::getBody()
	Get the body of the parsed HttpMessage. */
PHP_METHOD(HttpMessage, getBody)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);
		RETURN_PHPSTR(&obj->message->body, PHPSTR_FREE_NOT, 1);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setBody(string body)
	Set the body of the HttpMessage. NOTE: Don't forget to update any headers accordingly. */
PHP_METHOD(HttpMessage, setBody)
{
	char *body;
	int len;
	getObject(http_message_object, obj);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &body, &len)) {
		phpstr_dtor(PHPSTR(obj->message));
		phpstr_from_string_ex(PHPSTR(obj->message), body, len);
	}
}
/* }}} */

/* {{{ proto string HttpMessage::getHeader(string header)
	Get message header. */
PHP_METHOD(HttpMessage, getHeader)
{
	zval *header;
	char *orig_header, *nice_header;
	int header_len;
	getObject(http_message_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &orig_header, &header_len)) {
		RETURN_FALSE;
	}
	
	nice_header = pretty_key(estrndup(orig_header, header_len), header_len, 1, 1);
	if ((header = http_message_header_ex(obj->message, nice_header, header_len + 1, 0))) {
		RETVAL_ZVAL(header, 1, 1);
	}
	efree(nice_header);
}
/* }}} */

/* {{{ proto array HttpMessage::getHeaders()
	Get Message Headers. */
PHP_METHOD(HttpMessage, getHeaders)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);

		array_init(return_value);
		array_copy(&obj->message->hdrs, Z_ARRVAL_P(return_value));
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setHeaders(array headers)
	Sets new headers. */
PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers = NULL;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/!", &new_headers)) {
		return;
	}

	zend_hash_clean(&obj->message->hdrs);
	if (new_headers) {
		array_copy(Z_ARRVAL_P(new_headers), &obj->message->hdrs);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::addHeaders(array headers[, bool append = false])
	Add headers. If append is true, headers with the same name will be separated, else overwritten. */
PHP_METHOD(HttpMessage, addHeaders)
{
	zval *new_headers;
	zend_bool append = 0;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		return;
	}

	array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, append, ARRAY_JOIN_STRONLY|ARRAY_JOIN_PRETTIFY);
}
/* }}} */

/* {{{ proto int HttpMessage::getType()
	Get Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE) */
PHP_METHOD(HttpMessage, getType)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);
		RETURN_LONG(obj->message->type);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setType(int type)
	Set Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE) */
PHP_METHOD(HttpMessage, setType)
{
	long type;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type)) {
		return;
	}
	http_message_set_type(obj->message, type);
}
/* }}} */

/* {{{ proto string HttpMessage::getInfo(void)
	Get the HTTP request/response line */
PHP_METHOD(HttpMessage, getInfo)
{
	NO_ARGS;
	
	if (return_value_used) {
		getObject(http_message_object, obj);
		
		switch (obj->message->type) {
			case HTTP_MSG_REQUEST:
				Z_STRLEN_P(return_value) = spprintf(&Z_STRVAL_P(return_value), 0, HTTP_INFO_REQUEST_FMT_ARGS(&obj->message->http, ""));
				break;
			case HTTP_MSG_RESPONSE:
				Z_STRLEN_P(return_value) = spprintf(&Z_STRVAL_P(return_value), 0, HTTP_INFO_RESPONSE_FMT_ARGS(&obj->message->http, ""));
				break;
			default:
				RETURN_NULL();
				break;
		}
		Z_TYPE_P(return_value) = IS_STRING;
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setInfo(string http_info)
	Set type and request or response info with a standard HTTP request or response line */
PHP_METHOD(HttpMessage, setInfo)
{
	char *str;
	int len;
	http_info inf;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len) && SUCCESS == http_info_parse_ex(str, &inf, 0)) {
		getObject(http_message_object, obj);
		
		http_message_set_info(obj->message, &inf);
		http_info_dtor(&inf);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto int HttpMessage::getResponseCode()
	Get the Response Code of the Message. */
PHP_METHOD(HttpMessage, getResponseCode)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
		RETURN_LONG(obj->message->http.info.response.code);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseCode(int code)
	Set the response code of an HTTP Response Message. */
PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	getObject(http_message_object, obj);

	HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_FALSE;
	}
	if (code < 100 || code > 599) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid response code (100-599): %ld", code);
		RETURN_FALSE;
	}

	obj->message->http.info.response.code = code;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getResponseStatus()
	Get the Response Status of the message (i.e. the string following the response code). */
PHP_METHOD(HttpMessage, getResponseStatus)
{
	NO_ARGS;
	
	if (return_value_used) {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
		if (obj->message->http.info.response.status) {
			RETURN_STRING(obj->message->http.info.response.status, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseStatus(string status)
	Set the Response Status of the HTTP message (i.e. the string following the response code). */
PHP_METHOD(HttpMessage, setResponseStatus)
{
	char *status;
	int status_len;
	getObject(http_message_object, obj);
	
	HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &status, &status_len)) {
		RETURN_FALSE;
	}
	STR_SET(obj->message->http.info.response.status, estrndup(status, status_len));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestMethod()
	Get the Request Method of the Message. */
PHP_METHOD(HttpMessage, getRequestMethod)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.method) {
			RETURN_STRING(obj->message->http.info.request.method, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestMethod(string method)
	Set the Request Method of the HTTP Message. */
PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;
	getObject(http_message_object, obj);

	HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if (method_len < 1) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
		RETURN_FALSE;
	}
	if (!http_request_method_exists(1, 0, method)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Unknown request method: %s", method);
		RETURN_FALSE;
	}

	STR_SET(obj->message->http.info.request.method, estrndup(method, method_len));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestUrl()
	Get the Request URL of the Message. */
PHP_METHOD(HttpMessage, getRequestUrl)
{
	NO_ARGS;

	if (return_value_used) {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.url) {
			RETURN_STRING(obj->message->http.info.request.url, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestUrl(string url)
	Set the Request URL of the HTTP Message. */
PHP_METHOD(HttpMessage, setRequestUrl)
{
	char *URI;
	int URIlen;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URI, &URIlen)) {
		RETURN_FALSE;
	}
	HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
	if (URIlen < 1) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestUrl to an empty string");
		RETURN_FALSE;
	}

	STR_SET(obj->message->http.info.request.url, estrndup(URI, URIlen));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getHttpVersion()
	Get the HTTP Protocol Version of the Message. */
PHP_METHOD(HttpMessage, getHttpVersion)
{
	NO_ARGS;

	if (return_value_used) {
		char *version;
		getObject(http_message_object, obj);

		spprintf(&version, 0, "%1.1F", obj->message->http.version);
		RETURN_STRING(version, 0);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setHttpVersion(string version)
	Set the HTTP Protocol version of the Message. */
PHP_METHOD(HttpMessage, setHttpVersion)
{
	zval *zv;
	char *version;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &zv)) {
		return;
	}

	convert_to_double(zv);
	spprintf(&version, 0, "%1.1F", Z_DVAL_P(zv));
	if (strcmp(version, "1.0") && strcmp(version, "1.1")) {
		efree(version);
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid HTTP protocol version (1.0 or 1.1): %g", Z_DVAL_P(zv));
		RETURN_FALSE;
	}
	efree(version);
	obj->message->http.version = Z_DVAL_P(zv);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::guessContentType(string magic_file[, int magic_mode = MAGIC_MIME])
	Attempts to guess the content type of supplied payload through libmagic. */
PHP_METHOD(HttpMessage, guessContentType)
{
#ifdef HTTP_HAVE_MAGIC
	char *magic_file, *ct = NULL;
	int magic_file_len;
	long magic_mode = MAGIC_MIME;
	
	RETVAL_FALSE;
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &magic_file, &magic_file_len, &magic_mode)) {
		getObject(http_message_object, obj);
		if ((ct = http_guess_content_type(magic_file, magic_mode, PHPSTR_VAL(&obj->message->body), PHPSTR_LEN(&obj->message->body), SEND_DATA))) {
			RETVAL_STRING(ct, 0);
		}
	}
	SET_EH_NORMAL();
#else
	http_error(HE_THROW, HTTP_E_RUNTIME, "Cannot guess Content-Type; libmagic not available");
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::getParentMessage()
	Get parent Message. */
PHP_METHOD(HttpMessage, getParentMessage)
{
	SET_EH_THROW_HTTP();
	NO_ARGS {
		getObject(http_message_object, obj);

		if (obj->message->parent) {
			RETVAL_OBJVAL(obj->parent, 1);
		} else {
			http_error(HE_WARNING, HTTP_E_RUNTIME, "HttpMessage does not have a parent message");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto bool HttpMessage::send()
	Send the Message according to its type as Response or Request. */
PHP_METHOD(HttpMessage, send)
{
	getObject(http_message_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_message_send(obj->message));
}
/* }}} */

/* {{{ proto string HttpMessage::toString([bool include_parent = false])
	Get the string representation of the Message. */
PHP_METHOD(HttpMessage, toString)
{
	if (return_value_used) {
		char *string;
		size_t length;
		zend_bool include_parent = 0;
		getObject(http_message_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &include_parent)) {
			RETURN_FALSE;
		}

		if (include_parent) {
			http_message_serialize(obj->message, &string, &length);
		} else {
			http_message_tostring(obj->message, &string, &length);
		}
		RETURN_STRINGL(string, length, 0);
	}
}
/* }}} */

/* {{{ proto HttpRequest|HttpResponse HttpMessage::toMessageTypeObject(void)
	Creates an object regarding to the type of the message. Returns either an HttpRequest or HttpResponse object on success, or NULL on failure. */
PHP_METHOD(HttpMessage, toMessageTypeObject)
{
	SET_EH_THROW_HTTP();
	
	NO_ARGS;
	
	if (return_value_used) {
		getObject(http_message_object, obj);
		
		switch (obj->message->type) {
			case HTTP_MSG_REQUEST:
			{
#ifdef HTTP_HAVE_CURL
				int method;
				char *url;
				zval post, body, *array, *headers, *host = http_message_header(obj->message, "Host");
				php_url hurl, *purl = php_url_parse(STR_PTR(obj->message->http.info.request.url));
				
				MAKE_STD_ZVAL(array);
				array_init(array);
				
				memset(&hurl, 0, sizeof(php_url));
				if (host) {
					hurl.host = Z_STRVAL_P(host);
					zval_ptr_dtor(&host);
				}
				http_build_url(HTTP_URL_REPLACE, purl, &hurl, NULL, &url, NULL);
				php_url_free(purl);
				add_assoc_string(array, "url", url, 0);
				
				if (	obj->message->http.info.request.method &&
							((method = http_request_method_exists(1, 0, obj->message->http.info.request.method)) ||
							(method = http_request_method_register(obj->message->http.info.request.method, strlen(obj->message->http.info.request.method))))) {
					add_assoc_long(array, "method", method);
				}
				
				if (10 == (int) (obj->message->http.version * 10)) {
					add_assoc_long(array, "protocol", CURL_HTTP_VERSION_1_0);
				}
				
				MAKE_STD_ZVAL(headers);
				array_init(headers);
				array_copy(&obj->message->hdrs, Z_ARRVAL_P(headers));
				add_assoc_zval(array, "headers", headers);
				
				object_init_ex(return_value, http_request_object_ce);
				zend_call_method_with_1_params(&return_value, http_request_object_ce, NULL, "setoptions", NULL, array);
				zval_ptr_dtor(&array);
				
				if (PHPSTR_VAL(obj->message) && PHPSTR_LEN(obj->message)) {
					phpstr_fix(PHPSTR(obj->message));
					INIT_PZVAL(&body);
					ZVAL_STRINGL(&body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message), 0);
					if (method != HTTP_POST) {
						zend_call_method_with_1_params(&return_value, http_request_object_ce, NULL, "setbody", NULL, &body);
					} else {
						INIT_PZVAL(&post);
						array_init(&post);
						
						zval_copy_ctor(&body);
						sapi_module.treat_data(PARSE_STRING, Z_STRVAL(body), &post TSRMLS_CC);
						zend_call_method_with_1_params(&return_value, http_request_object_ce, NULL, "setpostfields", NULL, &post);
						zval_dtor(&post);
					}
				}
#else
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot transform HttpMessage to HttpRequest (missing curl support)");
#endif
				break;
			}
			
			case HTTP_MSG_RESPONSE:
			{
#ifndef WONKY
				HashPosition pos1, pos2;
				HashKey key = initHashKey(0);
				zval **header, **h, *body;
				
				if (obj->message->http.info.response.code) {
					http_send_status(obj->message->http.info.response.code);
				}
				
				object_init_ex(return_value, http_response_object_ce);
				
				FOREACH_HASH_KEYVAL(pos1, &obj->message->hdrs, key, header) {
					if (key.type == HASH_KEY_IS_STRING) {
						zval *zkey;
						
						MAKE_STD_ZVAL(zkey);
						ZVAL_STRINGL(zkey, key.str, key.len - 1, 1);
						
						switch (Z_TYPE_PP(header)) {
							case IS_ARRAY:
							case IS_OBJECT:
								FOREACH_HASH_VAL(pos2, HASH_OF(*header), h) {
									ZVAL_ADDREF(*h);
									zend_call_method_with_2_params(&return_value, http_response_object_ce, NULL, "setheader", NULL, zkey, *h);
									zval_ptr_dtor(h);
								}
								break;
							
							default:
								ZVAL_ADDREF(*header);
								zend_call_method_with_2_params(&return_value, http_response_object_ce, NULL, "setheader", NULL, zkey, *header);
								zval_ptr_dtor(header);
								break;
						}
						zval_ptr_dtor(&zkey);
					}
				}
				
				MAKE_STD_ZVAL(body);
				ZVAL_STRINGL(body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message), 1);
				zend_call_method_with_1_params(&return_value, http_response_object_ce, NULL, "setdata", NULL, body);
				zval_ptr_dtor(&body);
#else
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot transform HttpMessage to HttpResponse (need PHP 5.1+)");
#endif
				break;
			}
			
			default:
				http_error(HE_WARNING, HTTP_E_MESSAGE_TYPE, "HttpMessage is neither of type HttpMessage::TYPE_REQUEST nor HttpMessage::TYPE_RESPONSE");
				break;
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto int HttpMessage::count()
	Implements Countable::count(). Returns the number of parent messages + 1. */
PHP_METHOD(HttpMessage, count)
{
	NO_ARGS {
		long i;
		getObject(http_message_object, obj);
		
		http_message_count(i, obj->message);
		RETURN_LONG(i);
	}
}
/* }}} */

/* {{{ proto string HttpMessage::serialize()
	Implements Serializable::serialize(). Returns the serialized representation of the HttpMessage. */
PHP_METHOD(HttpMessage, serialize)
{
	NO_ARGS {
		char *string;
		size_t length;
		getObject(http_message_object, obj);
		
		http_message_serialize(obj->message, &string, &length);
		RETURN_STRINGL(string, length, 0);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::unserialize(string serialized)
	Implements Serializable::unserialize(). Re-constructs the HttpMessage based upon the serialized string. */
PHP_METHOD(HttpMessage, unserialize)
{
	int length;
	char *serialized;
	getObject(http_message_object, obj);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &serialized, &length)) {
		http_message *msg;
		
		http_message_dtor(obj->message);
		if ((msg = http_message_parse_ex(obj->message, serialized, (size_t) length))) {
			obj->message = msg;
		} else {
			http_message_init(obj->message);
			http_error(HE_ERROR, HTTP_E_RUNTIME, "Could not unserialize HttpMessage");
		}
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::detach(void)
	Returns a clone of an HttpMessage object detached from any parent messages. */
PHP_METHOD(HttpMessage, detach)
{
	http_info info;
	http_message *msg;
	getObject(http_message_object, obj);
	
	NO_ARGS;
	
	info.type = obj->message->type;
	memcpy(&HTTP_INFO(&info), &HTTP_INFO(obj->message), sizeof(struct http_info));
	
	msg = http_message_new();
	http_message_set_info(msg, &info);
	
	zend_hash_copy(&msg->hdrs, &obj->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	phpstr_append(&msg->body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message));
	
	RETVAL_OBJVAL(http_message_object_new_ex(Z_OBJCE_P(getThis()), msg, NULL), 0);
}
/* }}} */

/* {{{ proto void HttpMessage::prepend(HttpMessage message[, bool top = true])
	Prepends message(s) to the HTTP message. Throws HttpInvalidParamException if the message is located within the same message chain. */
PHP_METHOD(HttpMessage, prepend)
{
	zval *prepend;
	zend_bool top = 1;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|b", &prepend, http_message_object_ce, &top)) {
		http_message *msg[2];
		getObject(http_message_object, obj);
		getObjectEx(http_message_object, prepend_obj, prepend);
		
		/* safety check */
		for (msg[0] = obj->message; msg[0]; msg[0] = msg[0]->parent) {
			for (msg[1] = prepend_obj->message; msg[1]; msg[1] = msg[1]->parent) {
				if (msg[0] == msg[1]) {
					http_error(HE_THROW, HTTP_E_INVALID_PARAM, "Cannot prepend a message located within the same message chain");
					return;
				}
			}
		}
		
		http_message_object_prepend_ex(getThis(), prepend, top);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::reverse()
	Reorders the message chain in reverse order. Returns the most parent HttpMessage object. */
PHP_METHOD(HttpMessage, reverse)
{
	NO_ARGS {
		http_message_object_reverse(getThis(), return_value);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::rewind(void)
	Implements Iterator::rewind(). */
PHP_METHOD(HttpMessage, rewind)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		if (obj->iterator) {
			zval_ptr_dtor(&obj->iterator);
		}
		ZVAL_ADDREF(getThis());
		obj->iterator = getThis();
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::valid(void)
	Implements Iterator::valid(). */
PHP_METHOD(HttpMessage, valid)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		RETURN_BOOL(obj->iterator != NULL);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::next(void)
	Implements Iterator::next(). */
PHP_METHOD(HttpMessage, next)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		if (obj->iterator) {
			getObjectEx(http_message_object, itr, obj->iterator);
			
			if (itr && itr->parent.handle) {
				zval *old = obj->iterator;
				MAKE_STD_ZVAL(obj->iterator);
				ZVAL_OBJVAL(obj->iterator, itr->parent, 1);
				zval_ptr_dtor(&old);
			} else {
				zval_ptr_dtor(&obj->iterator);
				obj->iterator = NULL;
			}
		}
	}
}
/* }}} */

/* {{{ proto int HttpMessage::key(void)
	Implements Iterator::key(). */
PHP_METHOD(HttpMessage, key)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		RETURN_LONG(obj->iterator ? obj->iterator->value.obj.handle:0);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::current(void)
	Implements Iterator::current(). */
PHP_METHOD(HttpMessage, current)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		if (obj->iterator) {
			RETURN_ZVAL(obj->iterator, 1, 0);
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

