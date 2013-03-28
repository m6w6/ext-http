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

#ifndef PHP_HTTP_MESSAGE_H
#define PHP_HTTP_MESSAGE_H

#include "php_http_message_body.h"

/* required minimum length of an HTTP message "HTTP/1.1" */
#define PHP_HTTP_MESSAGE_MIN_SIZE 8
#define PHP_HTTP_MESSAGE_TYPE(TYPE, msg) ((msg) && ((msg)->type == PHP_HTTP_ ##TYPE))
#define PHP_HTTP_MESSAGE_TYPE_CHECK(type, msg, action) \
		if (!PHP_HTTP_MESSAGE_TYPE(type, (msg))) { \
			php_http_error(HE_NOTICE, PHP_HTTP_E_MESSAGE_TYPE, "HttpMessage is not of type "#type); \
			action; \
		}

typedef php_http_info_type_t php_http_message_type_t;
typedef struct php_http_message php_http_message_t;

struct php_http_message {
	PHP_HTTP_INFO_IMPL(http, type)
	HashTable hdrs;
	php_http_message_body_t *body;
	php_http_message_t *parent;
	void *opaque;
#ifdef ZTS
	void ***ts;
#endif
};

PHP_HTTP_API zend_bool php_http_message_info_callback(php_http_message_t **message, HashTable **headers, php_http_info_t *info TSRMLS_DC);

PHP_HTTP_API php_http_message_t *php_http_message_init(php_http_message_t *m, php_http_message_type_t t, php_http_message_body_t *body TSRMLS_DC);
PHP_HTTP_API php_http_message_t *php_http_message_init_env(php_http_message_t *m, php_http_message_type_t t TSRMLS_DC);
PHP_HTTP_API php_http_message_t *php_http_message_copy(php_http_message_t *from, php_http_message_t *to);
PHP_HTTP_API php_http_message_t *php_http_message_copy_ex(php_http_message_t *from, php_http_message_t *to, zend_bool parents);
PHP_HTTP_API void php_http_message_dtor(php_http_message_t *message);
PHP_HTTP_API void php_http_message_free(php_http_message_t **message);

PHP_HTTP_API void php_http_message_set_type(php_http_message_t *m, php_http_message_type_t t);
PHP_HTTP_API void php_http_message_set_info(php_http_message_t *message, php_http_info_t *info);

PHP_HTTP_API void php_http_message_update_headers(php_http_message_t *msg);

PHP_HTTP_API zval *php_http_message_header(php_http_message_t *msg, const char *key_str, size_t key_len, int join);
PHP_HTTP_API zend_bool php_http_message_is_multipart(php_http_message_t *msg, char **boundary);

PHP_HTTP_API void php_http_message_to_string(php_http_message_t *msg, char **string, size_t *length);
PHP_HTTP_API void php_http_message_to_struct(php_http_message_t *msg, zval *strct);
PHP_HTTP_API void php_http_message_to_callback(php_http_message_t *msg, php_http_pass_callback_t cb, void *cb_arg);

PHP_HTTP_API void php_http_message_serialize(php_http_message_t *message, char **string, size_t *length);
PHP_HTTP_API php_http_message_t *php_http_message_reverse(php_http_message_t *msg);
PHP_HTTP_API php_http_message_t *php_http_message_zip(php_http_message_t *one, php_http_message_t *two);

#define php_http_message_count(c, m) \
{ \
	php_http_message_t *__tmp_msg = (m); \
	for (c = 0; __tmp_msg; __tmp_msg = __tmp_msg->parent, ++(c)); \
}

PHP_HTTP_API php_http_message_t *php_http_message_parse(php_http_message_t *msg, const char *str, size_t len, zend_bool greedy TSRMLS_DC);

/* PHP */

typedef struct php_http_message_object {
	zend_object zo;
	zend_object_value zv;
	php_http_message_t *message;
	zend_object_value parent, body;
	zval *iterator;
} php_http_message_object_t;

zend_class_entry *php_http_message_get_class_entry(void);

PHP_MINIT_FUNCTION(http_message);
PHP_MSHUTDOWN_FUNCTION(http_message);

void php_http_message_object_prepend(zval *this_ptr, zval *prepend, zend_bool top /* = 1 */ TSRMLS_DC);
void php_http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC);
STATUS php_http_message_object_set_body(php_http_message_object_t *obj, zval *zbody TSRMLS_DC);

zend_object_value php_http_message_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_message_object_new_ex(zend_class_entry *ce, php_http_message_t *msg, php_http_message_object_t **ptr TSRMLS_DC);
zend_object_value php_http_message_object_clone(zval *object TSRMLS_DC);
void php_http_message_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpMessage, __construct);
PHP_METHOD(HttpMessage, getBody);
PHP_METHOD(HttpMessage, setBody);
PHP_METHOD(HttpMessage, addBody);
PHP_METHOD(HttpMessage, getHeader);
PHP_METHOD(HttpMessage, setHeader);
PHP_METHOD(HttpMessage, addHeader);
PHP_METHOD(HttpMessage, getHeaders);
PHP_METHOD(HttpMessage, setHeaders);
PHP_METHOD(HttpMessage, addHeaders);
PHP_METHOD(HttpMessage, getType);
PHP_METHOD(HttpMessage, setType);
PHP_METHOD(HttpMessage, getInfo);
PHP_METHOD(HttpMessage, setInfo);
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
PHP_METHOD(HttpMessage, toStream);
PHP_METHOD(HttpMessage, toCallback);

PHP_METHOD(HttpMessage, count);
PHP_METHOD(HttpMessage, serialize);
PHP_METHOD(HttpMessage, unserialize);
PHP_METHOD(HttpMessage, rewind);
PHP_METHOD(HttpMessage, valid);
PHP_METHOD(HttpMessage, current);
PHP_METHOD(HttpMessage, key);
PHP_METHOD(HttpMessage, next);

PHP_METHOD(HttpMessage, factory);

PHP_METHOD(HttpMessage, detach);
PHP_METHOD(HttpMessage, prepend);
PHP_METHOD(HttpMessage, reverse);

PHP_METHOD(HttpMessage, isMultipart);
PHP_METHOD(HttpMessage, splitMultipartBody);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

