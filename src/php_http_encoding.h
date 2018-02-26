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

#ifndef PHP_HTTP_ENCODING_H
#define PHP_HTTP_ENCODING_H

extern PHP_MINIT_FUNCTION(http_encoding);

#define PHP_HTTP_ENCODING_STREAM_PERSISTENT	0x01000000
#define PHP_HTTP_ENCODING_STREAM_DIRTY		0x02000000

#define PHP_HTTP_ENCODING_STREAM_FLUSH_NONE	0x00000000
#define PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC 0x00100000
#define PHP_HTTP_ENCODING_STREAM_FLUSH_FULL 0x00200000

#define PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG(flags, full, sync, none) \
	(((flags) & PHP_HTTP_ENCODING_STREAM_FLUSH_FULL) ? (full) : \
	(((flags) & PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC) ? (sync) : (none)))

typedef struct php_http_encoding_stream php_http_encoding_stream_t;

typedef php_http_encoding_stream_t *(*php_http_encoding_stream_init_func_t)(php_http_encoding_stream_t *s);
typedef php_http_encoding_stream_t *(*php_http_encoding_stream_copy_func_t)(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to);
typedef ZEND_RESULT_CODE (*php_http_encoding_stream_update_func_t)(php_http_encoding_stream_t *s, const char *in_str, size_t in_len, char **out_str, size_t *out_len);
typedef ZEND_RESULT_CODE (*php_http_encoding_stream_flush_func_t)(php_http_encoding_stream_t *s, char **out_str, size_t *out_len);
typedef zend_bool (*php_http_encoding_stream_done_func_t)(php_http_encoding_stream_t *s);
typedef ZEND_RESULT_CODE (*php_http_encoding_stream_finish_func_t)(php_http_encoding_stream_t *s, char **out_str, size_t *out_len);
typedef void (*php_http_encoding_stream_dtor_func_t)(php_http_encoding_stream_t *s);

typedef struct php_http_encoding_stream_ops {
	php_http_encoding_stream_init_func_t init;
	php_http_encoding_stream_copy_func_t copy;
	php_http_encoding_stream_update_func_t update;
	php_http_encoding_stream_flush_func_t flush;
	php_http_encoding_stream_done_func_t done;
	php_http_encoding_stream_finish_func_t finish;
	php_http_encoding_stream_dtor_func_t dtor;
} php_http_encoding_stream_ops_t;

struct php_http_encoding_stream {
	unsigned flags;
	void *ctx;
	php_http_encoding_stream_ops_t *ops;
};


PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_init(php_http_encoding_stream_t *s, php_http_encoding_stream_ops_t *ops, unsigned flags);
PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_reset(php_http_encoding_stream_t **s);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_update(php_http_encoding_stream_t *s, const char *in_str, size_t in_len, char **out_str, size_t *out_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_flush(php_http_encoding_stream_t *s, char **out_str, size_t *len);
PHP_HTTP_API zend_bool php_http_encoding_stream_done(php_http_encoding_stream_t *s);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_finish(php_http_encoding_stream_t *s, char **out_str, size_t *len);
PHP_HTTP_API void php_http_encoding_stream_dtor(php_http_encoding_stream_t *s);
PHP_HTTP_API void php_http_encoding_stream_free(php_http_encoding_stream_t **s);

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_dechunk_ops(void);
PHP_HTTP_API const char *php_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len);
PHP_HTTP_API zend_class_entry *php_http_get_dechunk_stream_class_entry(void);

typedef struct php_http_encoding_stream_object {
	php_http_encoding_stream_t *stream;
	zend_object zo;
} php_http_encoding_stream_object_t;

PHP_HTTP_API zend_class_entry *php_http_get_encoding_stream_class_entry(void);

zend_object *php_http_encoding_stream_object_new(zend_class_entry *ce);
php_http_encoding_stream_object_t *php_http_encoding_stream_object_new_ex(zend_class_entry *ce, php_http_encoding_stream_t *s);
zend_object *php_http_encoding_stream_object_clone(zval *object);
void php_http_encoding_stream_object_free(zend_object *object);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
