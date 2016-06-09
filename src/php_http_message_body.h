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

#ifndef PHP_HTTP_MESSAGE_BODY_H
#define PHP_HTTP_MESSAGE_BODY_H

typedef struct php_http_message_body {
	php_stream_statbuf ssb;
	zend_resource *res;
	char *boundary;
	unsigned refcount;
} php_http_message_body_t;

struct php_http_message;

PHP_HTTP_API php_http_message_body_t *php_http_message_body_init(php_http_message_body_t **body, php_stream *stream);
PHP_HTTP_API unsigned php_http_message_body_addref(php_http_message_body_t *body);
PHP_HTTP_API php_http_message_body_t *php_http_message_body_copy(php_http_message_body_t *from, php_http_message_body_t *to);
PHP_HTTP_API ZEND_RESULT_CODE php_http_message_body_add_form(php_http_message_body_t *body, HashTable *fields, HashTable *files);
PHP_HTTP_API ZEND_RESULT_CODE php_http_message_body_add_form_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_message_body_add_form_file(php_http_message_body_t *body, const char *name, const char *ctype, const char *file, php_stream *stream);
PHP_HTTP_API void php_http_message_body_add_part(php_http_message_body_t *body, struct php_http_message *part);
PHP_HTTP_API size_t php_http_message_body_append(php_http_message_body_t *body, const char *buf, size_t len);
PHP_HTTP_API size_t php_http_message_body_appendf(php_http_message_body_t *body, const char *fmt, ...);
PHP_HTTP_API zend_string *php_http_message_body_to_string(php_http_message_body_t *body, off_t offset, size_t forlen);
PHP_HTTP_API ZEND_RESULT_CODE php_http_message_body_to_stream(php_http_message_body_t *body, php_stream *s, off_t offset, size_t forlen);
PHP_HTTP_API ZEND_RESULT_CODE php_http_message_body_to_callback(php_http_message_body_t *body, php_http_pass_callback_t cb, void *cb_arg, off_t offset, size_t forlen);
PHP_HTTP_API void php_http_message_body_free(php_http_message_body_t **body);
PHP_HTTP_API const php_stream_statbuf *php_http_message_body_stat(php_http_message_body_t *body);
PHP_HTTP_API char *php_http_message_body_etag(php_http_message_body_t *body);
PHP_HTTP_API const char *php_http_message_body_boundary(php_http_message_body_t *body);
PHP_HTTP_API struct php_http_message *php_http_message_body_split(php_http_message_body_t *body, const char *boundary);

static inline size_t php_http_message_body_size(php_http_message_body_t *b)
{
	return php_http_message_body_stat(b)->sb.st_size;
}

static inline time_t php_http_message_body_mtime(php_http_message_body_t *b)
{
	return php_http_message_body_stat(b)->sb.st_mtime;
}

static inline php_stream *php_http_message_body_stream(php_http_message_body_t *body)
{
	return body && body->res ? body->res->ptr : NULL;
}

static inline zend_resource *php_http_message_body_resource(php_http_message_body_t *body)
{
	return body ? body->res : NULL;
}

typedef struct php_http_message_body_object {
	php_http_message_body_t *body;
	zval *gc;
	zend_object zo;
} php_http_message_body_object_t;

PHP_HTTP_API zend_class_entry *php_http_get_message_body_class_entry(void);
PHP_MINIT_FUNCTION(http_message_body);

zend_object *php_http_message_body_object_new(zend_class_entry *ce);
php_http_message_body_object_t *php_http_message_body_object_new_ex(zend_class_entry *ce, php_http_message_body_t *body);
zend_object *php_http_message_body_object_clone(zval *object);
void php_http_message_body_object_free(zend_object *object);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

