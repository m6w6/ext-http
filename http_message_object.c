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

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_message_object.h"

#ifdef ZEND_ENGINE_2

#define http_message_object_declare_default_properties() _http_message_object_declare_default_properties(TSRMLS_C)
static inline void _http_message_object_declare_default_properties(TSRMLS_D);
#define http_message_object_read_prop _http_message_object_read_prop
static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC);
#define http_message_object_write_prop _http_message_object_write_prop
static void _http_message_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC);
#define http_message_object_get_props _http_message_object_get_props
static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC);

zend_class_entry *http_message_object_ce;
zend_function_entry http_message_object_fe[] = {
	PHP_ME(HttpMessage, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpMessage, setRaw, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getBody, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, addHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getResponseCode, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setResponseCode, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getRequestMethod, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setRequestMethod, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getRequestUri, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setRequestUri, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getHttpVersion, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setHttpVersion, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, toString, NULL, ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpMessage, fromString, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};
static zend_object_handlers http_message_object_handlers;

void _http_message_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpMessage, http_message_object, NULL, 0);

	HTTP_LONG_CONSTANT("HTTP_MSG_NONE", HTTP_MSG_NONE);
	HTTP_LONG_CONSTANT("HTTP_MSG_REQUEST", HTTP_MSG_REQUEST);
	HTTP_LONG_CONSTANT("HTTP_MSG_RESPONSE", HTTP_MSG_RESPONSE);

	http_message_object_handlers.read_property = http_message_object_read_prop;
	http_message_object_handlers.write_property = http_message_object_write_prop;
	http_message_object_handlers.get_properties = http_message_object_get_props;
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
	o->message = msg ? msg : http_message_new();

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_message_object_free, NULL TSRMLS_CC);
	ov.handlers = &http_message_object_handlers;

	return ov;
}

static inline void _http_message_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_message_object_ce;

	DCL_PROP(PROTECTED, long, type, HTTP_MSG_NONE);

	DCL_PROP(PROTECTED, string, raw, "");
	DCL_PROP(PROTECTED, string, body, "");

	DCL_PROP(PROTECTED, string, requestMethod, "");
	DCL_PROP(PROTECTED, string, requestUri, "");
	DCL_PROP(PROTECTED, long, responseCode, 0);

	DCL_PROP_N(PROTECTED, httpVersion);
	DCL_PROP_N(PROTECTED, headers);
	DCL_PROP_N(PROTECTED, nestedMessage);
}

static void _http_message_object_free(zend_object *object TSRMLS_DC)
{
	http_message_object *o = (http_message_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->message) {
		http_message_free(o->message);
	}
	zval_dtor(&o->_tmp_property);
	efree(o);
}

static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	zval *return_value = &obj->_tmp_property;

	if (!EG(scope) || !instanceof_function(EG(scope), obj->zo.ce TSRMLS_CC)) {
		zend_error(E_WARNING, "Cannot access protected property %s::$%s", obj->zo.ce->name, Z_STRVAL_P(member));
		return EG(uninitialized_zval_ptr);
	}

    zval_dtor(return_value);

#if 0
	fprintf(stderr, "Reading property: %s(%d==%d) (%lu)\n", Z_STRVAL_P(member), Z_STRLEN_P(member), strlen(Z_STRVAL_P(member)),
		zend_get_hash_value(Z_STRVAL_P(member), strlen(Z_STRVAL_P(member)) + 1)
	);
#endif

	switch (zend_get_hash_value(Z_STRVAL_P(member), strlen(Z_STRVAL_P(member)) + 1))
	{
		case HTTP_MSG_PROPHASH_TYPE:
			RETVAL_LONG(msg->type);
		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
			switch (msg->type)
			{
				case HTTP_MSG_NONE:
					RETVAL_NULL();
				break;

				case HTTP_MSG_REQUEST:
					RETVAL_DOUBLE(msg->info.request.http_version);
				break;

				case HTTP_MSG_RESPONSE:
					RETVAL_DOUBLE(msg->info.response.http_version);
				break;
			}
		break;

		case HTTP_MSG_PROPHASH_RAW:
			if (msg->raw) {
				if (msg->len) {
					RETVAL_STRINGL(msg->raw, msg->len, 1);
				} else {
					RETVAL_STRINGL("", 0, 1);
				}
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_BODY:
			phpstr_fix(PHPSTR(msg));
			RETVAL_PHPSTR(PHPSTR(msg), 0, 1);
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
			array_init(return_value);
			zend_hash_copy(Z_ARRVAL_P(return_value), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		break;

		case HTTP_MSG_PROPHASH_NESTED_MESSAGE:
			RETVAL_NULL();
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
			if (msg->type == HTTP_MSG_REQUEST && msg->info.request.method) {
				RETVAL_STRING(msg->info.request.method, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URI:
			if (msg->type == HTTP_MSG_REQUEST && msg->info.request.URI) {
				RETVAL_STRING(msg->info.request.URI, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
			if (msg->type == HTTP_MSG_RESPONSE) {
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

	if (!EG(scope) || !instanceof_function(EG(scope), obj->zo.ce TSRMLS_CC)) {
		zend_error(E_WARNING, "Cannot access protected property %s::$%s", obj->zo.ce->name, Z_STRVAL_P(member));
	}

#if 0
	fprintf(stderr, "Writing property: %s(%d==%d) (%lu)\n", Z_STRVAL_P(member), Z_STRLEN_P(member), strlen(Z_STRVAL_P(member)),
		zend_get_hash_value(Z_STRVAL_P(member), strlen(Z_STRVAL_P(member)) + 1)
	);
#endif

	switch (zend_get_hash_value(Z_STRVAL_P(member), strlen(Z_STRVAL_P(member)) + 1))
	{
		case HTTP_MSG_PROPHASH_TYPE:
			if (Z_LVAL_P(value) != msg->type) {
				if (msg->type == HTTP_MSG_REQUEST) {
					if (msg->info.request.method) {
						efree(msg->info.request.method);
					}
					if (msg->info.request.URI) {
						efree(msg->info.request.URI);
					}
				}
				msg->type = Z_LVAL_P(value);
				if (msg->type == HTTP_MSG_REQUEST) {
					msg->info.request.method = NULL;
					msg->info.request.URI = NULL;
				}
			}

		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
			switch (msg->type)
			{
				case HTTP_MSG_REQUEST:
					msg->info.request.http_version = (float) Z_DVAL_P(value);
				break;

				case HTTP_MSG_RESPONSE:
					msg->info.response.http_version = (float) Z_DVAL_P(value);
				break;
			}
		break;

		case HTTP_MSG_PROPHASH_RAW:
			http_message_dtor(msg);
			http_message_parse_ex(msg, Z_STRVAL_P(value), Z_STRLEN_P(value), 1);
		break;

		case HTTP_MSG_PROPHASH_BODY:
			phpstr_dtor(PHPSTR(msg));
			phpstr_from_string_ex(PHPSTR(msg), Z_STRVAL_P(value), Z_STRLEN_P(value));
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
			zend_hash_clean(&msg->hdrs);
			zend_hash_copy(&msg->hdrs, Z_ARRVAL_P(value), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		break;

		case HTTP_MSG_PROPHASH_NESTED_MESSAGE:
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
			if (msg->type == HTTP_MSG_REQUEST) {
				if (msg->info.request.method) {
					efree(msg->info.request.method);
				}
				msg->info.request.method = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URI:
			if (msg->type == HTTP_MSG_REQUEST) {
				if (msg->info.request.URI) {
					efree(msg->info.request.URI);
				}
				msg->info.request.URI = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
			if (msg->type == HTTP_MSG_RESPONSE) {
				msg->info.response.code = Z_LVAL_P(value);
			}
		break;
	}
	zval_dtor(value);
	FREE_ZVAL(value);
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
	ASSOC_STRINGL(obj, "raw", msg->raw, msg->len)
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

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

