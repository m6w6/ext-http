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

#ifndef PHP_HTTP_MESSAGE_H
#define PHP_HTTP_MESSAGE_H

#include "php_http_message_body.h"
#include "php_http_header.h"

/* required minimum length of an HTTP message "HTTP/1.1" */
#define PHP_HTTP_MESSAGE_MIN_SIZE 8
#define PHP_HTTP_MESSAGE_TYPE(TYPE, msg) ((msg) && ((msg)->type == PHP_HTTP_ ##TYPE))

typedef php_http_info_type_t php_http_message_type_t;
typedef struct php_http_message php_http_message_t;

struct php_http_message {
	PHP_HTTP_INFO_IMPL(http, type)
	HashTable hdrs;
	php_http_message_body_t *body;
	php_http_message_t *parent;
	void *opaque;
};

PHP_HTTP_API zend_bool php_http_message_info_callback(php_http_message_t **message, HashTable **headers, php_http_info_t *info);

PHP_HTTP_API php_http_message_t *php_http_message_init(php_http_message_t *m, php_http_message_type_t t, php_http_message_body_t *body);
PHP_HTTP_API php_http_message_t *php_http_message_init_env(php_http_message_t *m, php_http_message_type_t t);
PHP_HTTP_API php_http_message_t *php_http_message_copy_ex(php_http_message_t *from, php_http_message_t *to, zend_bool parents);
static inline php_http_message_t *php_http_message_copy(php_http_message_t *from, php_http_message_t *to)
{
	return php_http_message_copy_ex(from, to, 1);
}

PHP_HTTP_API void php_http_message_dtor(php_http_message_t *message);
PHP_HTTP_API void php_http_message_free(php_http_message_t **message);

PHP_HTTP_API void php_http_message_set_type(php_http_message_t *m, php_http_message_type_t t);
PHP_HTTP_API void php_http_message_set_info(php_http_message_t *message, php_http_info_t *info);

PHP_HTTP_API void php_http_message_update_headers(php_http_message_t *msg);

PHP_HTTP_API zval *php_http_message_header(php_http_message_t *msg, const char *key_str, size_t key_len);

static inline zend_string *php_http_message_header_string(php_http_message_t *msg, const char *key_str, size_t key_len)
{
	zval *header;

	if ((header = php_http_message_header(msg, key_str, key_len))) {
		return php_http_header_value_to_string(header);
	}
	return NULL;
}

PHP_HTTP_API zend_bool php_http_message_is_multipart(php_http_message_t *msg, char **boundary);

PHP_HTTP_API void php_http_message_to_string(php_http_message_t *msg, char **string, size_t *length);
PHP_HTTP_API void php_http_message_to_struct(php_http_message_t *msg, zval *strct);
PHP_HTTP_API void php_http_message_to_callback(php_http_message_t *msg, php_http_pass_callback_t cb, void *cb_arg);

PHP_HTTP_API void php_http_message_serialize(php_http_message_t *message, char **string, size_t *length);
PHP_HTTP_API php_http_message_t *php_http_message_reverse(php_http_message_t *msg);
PHP_HTTP_API php_http_message_t *php_http_message_zip(php_http_message_t *one, php_http_message_t *two);

static inline size_t php_http_message_count(php_http_message_t *m)
{
	size_t c = 1;

	while ((m = m->parent)) {
		++c;
	}

	return c;
}

PHP_HTTP_API php_http_message_t *php_http_message_parse(php_http_message_t *msg, const char *str, size_t len, zend_bool greedy);

typedef struct php_http_message_object {
	php_http_message_t *message;
	struct php_http_message_object *parent;
	php_http_message_body_object_t *body;
	zval iterator, *gc;
	zend_object zo;
} php_http_message_object_t;

PHP_HTTP_API zend_class_entry *php_http_message_get_class_entry(void);

PHP_MINIT_FUNCTION(http_message);
PHP_MSHUTDOWN_FUNCTION(http_message);

void php_http_message_object_prepend(zval *this_ptr, zval *prepend, zend_bool top /* = 1 */);
void php_http_message_object_reverse(zval *this_ptr, zval *return_value);
ZEND_RESULT_CODE php_http_message_object_set_body(php_http_message_object_t *obj, zval *zbody);
ZEND_RESULT_CODE php_http_message_object_init_body_object(php_http_message_object_t *obj);

zend_object *php_http_message_object_new(zend_class_entry *ce);
php_http_message_object_t *php_http_message_object_new_ex(zend_class_entry *ce, php_http_message_t *msg);
zend_object *php_http_message_object_clone(zval *object);
void php_http_message_object_free(zend_object *object);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

