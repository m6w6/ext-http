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

#ifndef PHP_HTTP_MESSAGE_OBJECT_H
#define PHP_HTTP_MESSAGE_OBJECT_H
#ifdef ZEND_ENGINE_2

typedef struct _http_message_object_t {
	zend_object zo;
	http_message *message;
	zend_object_value parent;
	zval *iterator;
} http_message_object;

extern zend_class_entry *http_message_object_ce;
extern zend_function_entry http_message_object_fe[];

extern PHP_MINIT_FUNCTION(http_message_object);

#define http_message_object_prepend(o, p) http_message_object_prepend_ex((o), (p), 1)
#define http_message_object_prepend_ex(o, p, t) _http_message_object_prepend_ex((o), (p), (t) TSRMLS_CC)
extern void _http_message_object_prepend_ex(zval *this_ptr, zval *prepend, zend_bool top TSRMLS_DC);

#define http_message_object_reverse(t, r) _http_message_object_reverse((t), (r) TSRMLS_CC)
extern void _http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC);

#define http_message_object_new(ce) _http_message_object_new((ce) TSRMLS_CC)
extern zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC);
#define http_message_object_new_ex(ce, msg, ptr) _http_message_object_new_ex((ce), (msg), (ptr) TSRMLS_CC)
extern zend_object_value _http_message_object_new_ex(zend_class_entry *ce, http_message *msg, http_message_object **ptr TSRMLS_DC);
#define http_message_object_clone(zobj) _http_message_object_clone_obj(zobj TSRMLS_CC)
extern zend_object_value _http_message_object_clone_obj(zval *object TSRMLS_DC);
#define http_message_object_free(o) _http_message_object_free((o) TSRMLS_CC)
extern void _http_message_object_free(zend_object *object TSRMLS_DC);

#define HTTP_MSG_PROPHASH_TYPE                  276192743LU
#define HTTP_MSG_PROPHASH_HTTP_VERSION         1138628683LU
#define HTTP_MSG_PROPHASH_BODY                  254474387LU
#define HTTP_MSG_PROPHASH_HEADERS              3199929089LU
#define HTTP_MSG_PROPHASH_PARENT_MESSAGE       2105714836LU
#define HTTP_MSG_PROPHASH_REQUEST_METHOD       1669022159LU
#define HTTP_MSG_PROPHASH_REQUEST_URL          3208695585LU
#define HTTP_MSG_PROPHASH_RESPONSE_STATUS      3857097400LU
#define HTTP_MSG_PROPHASH_RESPONSE_CODE        1305615119LU

#define HTTP_MSG_CHILD_PROPHASH_TYPE            624467825LU
#define HTTP_MSG_CHILD_PROPHASH_HTTP_VERSION   1021966997LU
#define HTTP_MSG_CHILD_PROPHASH_BODY            602749469LU
#define HTTP_MSG_CHILD_PROPHASH_HEADERS        3626850379LU
#define HTTP_MSG_CHILD_PROPHASH_PARENT_MESSAGE 3910157662LU
#define HTTP_MSG_CHILD_PROPHASH_REQUEST_METHOD 3473464985LU
#define HTTP_MSG_CHILD_PROPHASH_REQUEST_URL    3855913003LU
#define HTTP_MSG_CHILD_PROPHASH_RESPONSE_STATUS 3274168514LU
#define HTTP_MSG_CHILD_PROPHASH_RESPONSE_CODE  1750746777LU

#define HTTP_MSG_CHECK_OBJ(obj, dofail) \
	if (!(obj)->message) { \
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is empty"); \
		dofail; \
	}
#define HTTP_MSG_CHECK_STD() HTTP_MSG_CHECK_OBJ(obj, RETURN_FALSE)

#define HTTP_MSG_INIT_OBJ(obj) \
	if (!(obj)->message) { \
		(obj)->message = http_message_new(); \
	}
#define HTTP_MSG_INIT_STD() HTTP_MSG_INIT_OBJ(obj)

PHP_METHOD(HttpMessage, __construct);
PHP_METHOD(HttpMessage, getBody);
PHP_METHOD(HttpMessage, setBody);
PHP_METHOD(HttpMessage, getHeader);
PHP_METHOD(HttpMessage, getHeaders);
PHP_METHOD(HttpMessage, setHeaders);
PHP_METHOD(HttpMessage, addHeaders);
PHP_METHOD(HttpMessage, getType);
PHP_METHOD(HttpMessage, setType);
PHP_METHOD(HttpMessage, getResponseCode);
PHP_METHOD(HttpMessage, setResponseCode);
PHP_METHOD(HttpMessage, getResponseStatus);
PHP_METHOD(HttpMessage, setResponseStatus);
PHP_METHOD(HttpMessage, getRequestMethod);
PHP_METHOD(HttpMessage, setRequestMethod);
PHP_METHOD(HttpMessage, getRequestUrl);
PHP_METHOD(HttpMessage, setRequestUrl);
PHP_METHOD(HttpMessage, getHttpVersion);
PHP_METHOD(HttpMessage, setHttpVersion);
PHP_METHOD(HttpMessage, guessContentType);
PHP_METHOD(HttpMessage, getParentMessage);
PHP_METHOD(HttpMessage, send);
PHP_METHOD(HttpMessage, toString);
PHP_METHOD(HttpMessage, toMessageTypeObject);

PHP_METHOD(HttpMessage, count);
PHP_METHOD(HttpMessage, serialize);
PHP_METHOD(HttpMessage, unserialize);
PHP_METHOD(HttpMessage, rewind);
PHP_METHOD(HttpMessage, valid);
PHP_METHOD(HttpMessage, current);
PHP_METHOD(HttpMessage, key);
PHP_METHOD(HttpMessage, next);

PHP_METHOD(HttpMessage, factory);
PHP_METHOD(HttpMessage, fromEnv);

PHP_METHOD(HttpMessage, detach);
PHP_METHOD(HttpMessage, prepend);
PHP_METHOD(HttpMessage, reverse);

#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

