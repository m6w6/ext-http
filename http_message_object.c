/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#ifdef ZEND_ENGINE_2

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_message_object.h"
#include "php_http_exception_object.h"

#include "phpstr/phpstr.h"

#define HTTP_BEGIN_ARGS(method, ret_ref, req_args) 	HTTP_BEGIN_ARGS_EX(HttpMessage, method, ret_ref, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)			HTTP_EMPTY_ARGS_EX(HttpMessage, method, ret_ref)
#define HTTP_MESSAGE_ME(method, visibility)			PHP_ME(HttpMessage, method, HTTP_ARGS(HttpMessage, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0, 0)
	HTTP_ARG_VAL(message, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(fromString, 1, 1)
	HTTP_ARG_VAL(message, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getBody, 0);
HTTP_EMPTY_ARGS(getHeaders, 0);
HTTP_BEGIN_ARGS(setHeaders, 0, 1)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addHeaders, 0, 1)
	HTTP_ARG_VAL(headers, 0)
	HTTP_ARG_VAL(append, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getType, 0);
HTTP_BEGIN_ARGS(setType, 0, 1)
	HTTP_ARG_VAL(type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseCode, 0);
HTTP_BEGIN_ARGS(setResponseCode, 0, 1)
	HTTP_ARG_VAL(response_code, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestMethod, 0);
HTTP_BEGIN_ARGS(setRequestMethod, 0, 1)
	HTTP_ARG_VAL(request_method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestUri, 0);
HTTP_BEGIN_ARGS(setRequestUri, 0, 1)
	HTTP_ARG_VAL(uri, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHttpVersion, 0);
HTTP_BEGIN_ARGS(setHttpVersion, 0, 1)
	HTTP_ARG_VAL(http_version, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getParentMessage, 1);
HTTP_EMPTY_ARGS(send, 0);
HTTP_BEGIN_ARGS(toString, 0, 0)
	HTTP_ARG_VAL(include_parent, 0)
HTTP_END_ARGS;

#define http_message_object_declare_default_properties() _http_message_object_declare_default_properties(TSRMLS_C)
static inline void _http_message_object_declare_default_properties(TSRMLS_D);
#define http_message_object_read_prop _http_message_object_read_prop
static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC);
#define http_message_object_write_prop _http_message_object_write_prop
static void _http_message_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC);
#define http_message_object_get_props _http_message_object_get_props
static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC);
#define http_message_object_clone_obj _http_message_object_clone_obj
static inline zend_object_value _http_message_object_clone_obj(zval *object TSRMLS_DC);

zend_class_entry *http_message_object_ce;
zend_function_entry http_message_object_fe[] = {
	HTTP_MESSAGE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_MESSAGE_ME(getBody, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(addHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestUri, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestUri, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getParentMessage, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(send, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(toString, ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, HTTP_ARGS(HttpMessage, toString), ZEND_ACC_PUBLIC)

	HTTP_MESSAGE_ME(fromString, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};
static zend_object_handlers http_message_object_handlers;

void _http_message_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpMessage, http_message_object, NULL, 0);

	HTTP_LONG_CONSTANT("HTTP_MSG_NONE", HTTP_MSG_NONE);
	HTTP_LONG_CONSTANT("HTTP_MSG_REQUEST", HTTP_MSG_REQUEST);
	HTTP_LONG_CONSTANT("HTTP_MSG_RESPONSE", HTTP_MSG_RESPONSE);

	http_message_object_handlers.clone_obj = http_message_object_clone_obj;
	http_message_object_handlers.read_property = http_message_object_read_prop;
	http_message_object_handlers.write_property = http_message_object_write_prop;
	http_message_object_handlers.get_properties = http_message_object_get_props;
	http_message_object_handlers.get_property_ptr_ptr = NULL;
}

zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_message_object_new_ex(ce, NULL);
}

zend_object_value _http_message_object_new_ex(zend_class_entry *ce, http_message *msg TSRMLS_DC)
{
	zend_object_value ov;
	http_message_object *o;

	o = ecalloc(1, sizeof(http_message_object));
	o->zo.ce = ce;
	o->message = NULL;
	o->parent.handle = 0;
	o->parent.handlers = NULL;

	if (msg) {
		o->message = msg;
		if (msg->parent) {
			o->parent = http_message_object_from_msg(msg->parent);
		}
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = putObject(http_message_object, o);
	ov.handlers = &http_message_object_handlers;

	return ov;
}

zend_object_value _http_message_object_clone(zval *this_ptr TSRMLS_DC)
{
	return http_message_object_clone_obj(this_ptr TSRMLS_CC);
}

static inline void _http_message_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_message_object_ce;

	DCL_PROP(PROTECTED, long, type, HTTP_MSG_NONE);
	DCL_PROP(PROTECTED, string, body, "");
	DCL_PROP(PROTECTED, string, requestMethod, "");
	DCL_PROP(PROTECTED, string, requestUri, "");
	DCL_PROP(PROTECTED, long, responseCode, 0);
	DCL_PROP_N(PROTECTED, httpVersion);
	DCL_PROP_N(PROTECTED, headers);
	DCL_PROP_N(PROTECTED, parentMessage);
}

void _http_message_object_free(zend_object *object TSRMLS_DC)
{
	http_message_object *o = (http_message_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->message) {
		http_message_dtor(o->message);
		efree(o->message);
	}
	efree(o);
}

static inline zend_object_value _http_message_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	getObject(http_message_object, obj);
	return http_message_object_from_msg(http_message_dup(obj->message));
}

static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	zval *return_value;

	return_value = &EG(uninitialized_zval);
	return_value->refcount = 0;
	return_value->is_ref = 0;

#if 0
	fprintf(stderr, "Read HttpMessage::$%s\n", Z_STRVAL_P(member));
#endif
	if (!EG(scope) || !instanceof_function(EG(scope), obj->zo.ce TSRMLS_CC)) {
		zend_error(E_WARNING, "Cannot access protected property %s::$%s", obj->zo.ce->name, Z_STRVAL_P(member));
		return EG(uninitialized_zval_ptr);
	}

	switch (zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member) + 1))
	{
		case HTTP_MSG_PROPHASH_TYPE:
			RETVAL_LONG(msg->type);
		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
			switch (msg->type)
			{
				case HTTP_MSG_REQUEST:
					RETVAL_DOUBLE(msg->info.request.http_version);
				break;

				case HTTP_MSG_RESPONSE:
					RETVAL_DOUBLE(msg->info.response.http_version);
				break;

				case HTTP_MSG_NONE:
				default:
					RETVAL_NULL();
				break;
			}
		break;

		case HTTP_MSG_PROPHASH_BODY:
			phpstr_fix(PHPSTR(msg));
			RETVAL_PHPSTR(PHPSTR(msg), 0, 1);
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
			/* This is needed for situations like
			 * $this->headers['foo'] = 'bar';
			 */
			if (type == BP_VAR_W) {
				return_value->refcount = 1;
				return_value->is_ref = 1;
				Z_TYPE_P(return_value) = IS_ARRAY;
				Z_ARRVAL_P(return_value) = &msg->hdrs;
			} else {
				array_init(return_value);
				zend_hash_copy(Z_ARRVAL_P(return_value), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
			}
		break;

		case HTTP_MSG_PROPHASH_PARENT_MESSAGE:
			if (msg->parent) {
				RETVAL_OBJVAL(obj->parent);
				Z_TYPE_P(return_value)	= IS_OBJECT;
				return_value->value.obj	= obj->parent;
				zend_objects_store_add_ref(return_value TSRMLS_CC);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
			if (HTTP_MSG_TYPE(REQUEST, msg) && msg->info.request.method) {
				RETVAL_STRING(msg->info.request.method, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URI:
			if (HTTP_MSG_TYPE(REQUEST, msg) && msg->info.request.URI) {
				RETVAL_STRING(msg->info.request.URI, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
			if (HTTP_MSG_TYPE(RESPONSE, msg)) {
				RETVAL_LONG(msg->info.response.code);
			} else {
				RETVAL_NULL();
			}
		break;

		default:
			RETVAL_NULL();
		break;
	}

	return return_value;
}

static void _http_message_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;

#if 0
	fprintf(stderr, "Write HttpMessage::$%s\n", Z_STRVAL_P(member));
#endif
	if (!EG(scope) || !instanceof_function(EG(scope), obj->zo.ce TSRMLS_CC)) {
		zend_error(E_WARNING, "Cannot access protected property %s::$%s", obj->zo.ce->name, Z_STRVAL_P(member));
	}

	switch (zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member) + 1))
	{
		case HTTP_MSG_PROPHASH_TYPE:
			convert_to_long_ex(&value);
			if ((http_message_type) Z_LVAL_P(value) != msg->type) {
				if (HTTP_MSG_TYPE(REQUEST, msg)) {
					if (msg->info.request.method) {
						efree(msg->info.request.method);
					}
					if (msg->info.request.URI) {
						efree(msg->info.request.URI);
					}
				}
				msg->type = Z_LVAL_P(value);
				if (HTTP_MSG_TYPE(REQUEST, msg)) {
					msg->info.request.method = NULL;
					msg->info.request.URI = NULL;
				}
			}

		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
			convert_to_double_ex(&value);
			switch (msg->type)
			{
				case HTTP_MSG_REQUEST:
					msg->info.request.http_version = Z_DVAL_P(value);
				break;

				case HTTP_MSG_RESPONSE:
					msg->info.response.http_version = Z_DVAL_P(value);
				break;
			}
		break;

		case HTTP_MSG_PROPHASH_BODY:
			convert_to_string_ex(&value);
			phpstr_dtor(PHPSTR(msg));
			phpstr_from_string_ex(PHPSTR(msg), Z_STRVAL_P(value), Z_STRLEN_P(value));
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
			convert_to_array_ex(&value);
			zend_hash_clean(&msg->hdrs);
			zend_hash_copy(&msg->hdrs, Z_ARRVAL_P(value), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		break;

		case HTTP_MSG_PROPHASH_PARENT_MESSAGE:
			if (msg->parent) {
				zval tmp;
				tmp.value.obj = obj->parent;
				zend_objects_store_del_ref(&tmp TSRMLS_CC);
			}
			zend_objects_store_add_ref(value TSRMLS_CC);
			obj->parent = value->value.obj;
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
			convert_to_string_ex(&value);
			if (HTTP_MSG_TYPE(REQUEST, msg)) {
				if (msg->info.request.method) {
					efree(msg->info.request.method);
				}
				msg->info.request.method = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URI:
			convert_to_string_ex(&value);
			if (HTTP_MSG_TYPE(REQUEST, msg)) {
				if (msg->info.request.URI) {
					efree(msg->info.request.URI);
				}
				msg->info.request.URI = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
			convert_to_long_ex(&value);
			if (HTTP_MSG_TYPE(RESPONSE, msg)) {
				msg->info.response.code = Z_LVAL_P(value);
			}
		break;
	}
}

static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC)
{
	zval *headers;
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;

#define ASSOC_PROP(obj, ptype, name, val) \
	{ \
		zval array; \
		char *m_prop_name; \
		int m_prop_len; \
		Z_ARRVAL(array) = OBJ_PROP(obj); \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 1); \
		add_assoc_ ##ptype## _ex(&array, m_prop_name, sizeof(name)+4, val); \
	}
#define ASSOC_STRING(obj, name, val) ASSOC_STRINGL(obj, name, val, strlen(val))
#define ASSOC_STRINGL(obj, name, val, len) \
	{ \
		zval array; \
		char *m_prop_name; \
		int m_prop_len; \
		Z_ARRVAL(array) = OBJ_PROP(obj); \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 1); \
		add_assoc_stringl_ex(&array, m_prop_name, sizeof(name)+4, val, len, 1); \
	}

	zend_hash_clean(OBJ_PROP(obj));

	ASSOC_PROP(obj, long, "type", msg->type);
	ASSOC_STRINGL(obj, "body", PHPSTR_VAL(msg), PHPSTR_LEN(msg));

	MAKE_STD_ZVAL(headers);
	array_init(headers);

	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	ASSOC_PROP(obj, zval, "headers", headers);

	switch (msg->type)
	{
		case HTTP_MSG_REQUEST:
			ASSOC_PROP(obj, double, "httpVersion", msg->info.request.http_version);
			ASSOC_PROP(obj, long, "responseCode", 0);
			ASSOC_STRING(obj, "requestMethod", msg->info.request.method);
			ASSOC_STRING(obj, "requestUri", msg->info.request.URI);
		break;

		case HTTP_MSG_RESPONSE:
			ASSOC_PROP(obj, double, "httpVersion", msg->info.response.http_version);
			ASSOC_PROP(obj, long, "responseCode", msg->info.response.code);
			ASSOC_STRING(obj, "requestMethod", "");
			ASSOC_STRING(obj, "requestUri", "");
		break;

		case HTTP_MSG_NONE:
		default:
			ASSOC_PROP(obj, double, "httpVersion", 0.0);
			ASSOC_PROP(obj, long, "responseCode", 0);
			ASSOC_STRING(obj, "requestMethod", "");
			ASSOC_STRING(obj, "requestUri", "");
		break;
	}

	return OBJ_PROP(obj);
}

/* ### USERLAND ### */

/* {{{ proto void HttpMessage::__construct([string message])
 *
 * Instantiate a new HttpMessage object.
 */
PHP_METHOD(HttpMessage, __construct)
{
	char *message = NULL;
	int length = 0;
	getObject(http_message_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &message, &length) && message && length) {
		if (obj->message = http_message_parse(message, length)) {
			if (obj->message->parent) {
				obj->parent = http_message_object_from_msg(obj->message->parent);
			}
		}
	} else if (!obj->message) {
		obj->message = http_message_new();
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto static HttpMessage HttpMessage::fromString(string raw_message)
 *
 * Create an HttpMessage object from a string.
 */
PHP_METHOD(HttpMessage, fromString)
{
	char *string = NULL;
	int length = 0;
	http_message *msg = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &string, &length)) {
		RETURN_NULL();
	}

	if (!(msg = http_message_parse(string, length))) {
		RETURN_NULL();
	}

	Z_TYPE_P(return_value) = IS_OBJECT;
	return_value->value.obj = http_message_object_from_msg(msg);
}
/* }}} */

/* {{{ proto string HttpMessage::getBody()
 *
 * Get the body of the parsed Message.
 */
PHP_METHOD(HttpMessage, getBody)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_PHPSTR(&obj->message->body, PHPSTR_FREE_NOT, 1);
	}
}
/* }}} */

/* {{{ proto array HttpMessage::getHeaders()
 *
 * Get Message Headers.
 */
PHP_METHOD(HttpMessage, getHeaders)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval headers;
		getObject(http_message_object, obj);

		Z_ARRVAL(headers) = &obj->message->hdrs;
		array_init(return_value);
		array_copy(&headers, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setHeaders(array headers)
 *
 * Sets new headers.
 */
PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers, old_headers;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_headers)) {
		return;
	}

	zend_hash_clean(&obj->message->hdrs);
	Z_ARRVAL(old_headers) = &obj->message->hdrs;
	array_copy(new_headers, &old_headers);
}
/* }}} */

/* {{{ proto void HttpMessage::addHeaders(array headers[, bool append = false])
 *
 * Add headers. If append is true, headers with the same name will be separated, else overwritten.
 */
PHP_METHOD(HttpMessage, addHeaders)
{
	zval old_headers, *new_headers;
	zend_bool append = 0;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		return;
	}

	Z_ARRVAL(old_headers) = &obj->message->hdrs;
	if (append) {
		array_append(new_headers, &old_headers);
	} else {
		array_merge(new_headers, &old_headers);
	}
}
/* }}} */

/* {{{ proto long HttpMessage::getType()
 *
 * Get Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 */
PHP_METHOD(HttpMessage, getType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_LONG(obj->message->type);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setType(long type)
 *
 * Set Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 */
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

/* {{{ proto long HttpMessage::getResponseCode()
 *
 * Get the Response Code of the Message.
 */
PHP_METHOD(HttpMessage, getResponseCode)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(RESPONSE, obj->message)) {
			http_error(E_NOTICE, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_RESPONSE");
			RETURN_NULL();
		}

		RETURN_LONG(obj->message->info.response.code);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseCode(long code)
 *
 * Set the response code of an HTTP Response Message.
 * Returns false if the Message is not of type HTTP_MSG_RESPONSE,
 * or if the response code is out of range (100-510).
 */
PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_RESPONSE");
		RETURN_FALSE;
	}

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_FALSE;
	}
	if (code < 100 || code > 510) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Invalid response code (100-510): %ld", code);
		RETURN_FALSE;
	}

	obj->message->info.response.code = code;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestMethod()
 *
 * Get the Request Method of the Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST.
 */
PHP_METHOD(HttpMessage, getRequestMethod)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
			http_error(E_NOTICE, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
			RETURN_NULL();
		}

		RETURN_STRING(obj->message->info.request.method, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestMethod(string method)
 *
 * Set the Request Method of the HTTP Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST.
 */
PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if (method_len < 1) {
		http_error(E_WARNING, HTTP_E_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
		RETURN_FALSE;
	}
	if (SUCCESS != http_check_method(method)) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Unkown request method: %s", method);
		RETURN_FALSE;
	}

	STR_SET(obj->message->info.request.method, estrndup(method, method_len));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestUri()
 *
 * Get the Request URI of the Message.
 */
PHP_METHOD(HttpMessage, getRequestUri)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
			http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
			RETURN_NULL();
		}

		RETURN_STRING(obj->message->info.request.URI, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestUri(string URI)
 *
 * Set the Request URI of the HTTP Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST,
 * or if paramtere URI was empty.
 */
PHP_METHOD(HttpMessage, setRequestUri)
{
	char *URI;
	int URIlen;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URI, &URIlen)) {
		RETURN_FALSE;
	}
	if (URIlen < 1) {
		http_error(E_WARNING, HTTP_E_PARAM, "Cannot set HttpMessage::requestUri to an empty string");
		RETURN_FALSE;
	}

	STR_SET(obj->message->info.request.URI, estrndup(URI, URIlen));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getHttpVersion()
 *
 * Get the HTTP Protocol Version of the Message.
 */
PHP_METHOD(HttpMessage, getHttpVersion)
{
	NO_ARGS;

	IF_RETVAL_USED {
		char ver[4] = {0};
		double version;
		getObject(http_message_object, obj);

		switch (obj->message->type)
		{
			case HTTP_MSG_RESPONSE:
				version = obj->message->info.response.http_version;
			break;

			case HTTP_MSG_REQUEST:
				version = obj->message->info.request.http_version;
			break;

			case HTTP_MSG_NONE:
			default:
				RETURN_NULL();
		}
		sprintf(ver, "%1.1lf", version);
		RETURN_STRINGL(ver, 3, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setHttpVersion(string version)
 *
 * Set the HTTP Protocol version of the Message.
 * Returns false if version is invalid (1.0 and 1.1).
 */
PHP_METHOD(HttpMessage, setHttpVersion)
{
	char v[4];
	zval *zv;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &zv)) {
		return;
	}

	if (HTTP_MSG_TYPE(NONE, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "Message is neither of type HTTP_MSG_RESPONSE nor HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}

	convert_to_double(zv);
	sprintf(v, "%1.1lf", Z_DVAL_P(zv));
	if (strcmp(v, "1.0") && strcmp(v, "1.1")) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Invalid HTTP protocol version (1.0 or 1.1): %s", v);
		RETURN_FALSE;
	}

	if (HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		obj->message->info.response.http_version = Z_DVAL_P(zv);
	} else {
		obj->message->info.request.http_version = Z_DVAL_P(zv);
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::getParentMessage()
 *
 * Get parent Message.
 */
PHP_METHOD(HttpMessage, getParentMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (obj->message->parent) {
			RETVAL_OBJVAL(obj->parent);
		} else {
			RETVAL_NULL();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::send()
 *
 * Send the Message according to its type as Response or Request.
 */
PHP_METHOD(HttpMessage, send)
{
	getObject(http_message_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_message_send(obj->message));
}
/* }}} */

/* {{{ proto string HttpMessage::toString([bool include_parent = true])
 *
 * Get the string representation of the Message.
 */
PHP_METHOD(HttpMessage, toString)
{
	IF_RETVAL_USED {
		char *string;
		size_t length;
		zend_bool include_parent = 1;
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

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

