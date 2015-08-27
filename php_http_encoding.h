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

#include <zlib.h>

extern PHP_MINIT_FUNCTION(http_encoding);
extern PHP_RINIT_FUNCTION(http_encoding);
extern PHP_RSHUTDOWN_FUNCTION(http_encoding);

#define PHP_HTTP_INFLATE_ROUNDS 100

#define PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(S) \
	(((size_t) ((double) S * (double) 1.015)) + 10 + 8 + 4 + 1)
#define PHP_HTTP_INFLATE_BUFFER_SIZE_GUESS(S) \
	(((S) + 1) << 3)
#define PHP_HTTP_INFLATE_BUFFER_SIZE_ALIGN(S) \
	((S) += (S) >> (3))

#define PHP_HTTP_DEFLATE_BUFFER_SIZE		0x8000
#define PHP_HTTP_INFLATE_BUFFER_SIZE		0x1000

#define PHP_HTTP_DEFLATE_LEVEL_DEF			0x00000000
#define PHP_HTTP_DEFLATE_LEVEL_MIN			0x00000001
#define PHP_HTTP_DEFLATE_LEVEL_MAX			0x00000009
#define PHP_HTTP_DEFLATE_TYPE_ZLIB			0x00000000
#define PHP_HTTP_DEFLATE_TYPE_GZIP			0x00000010
#define PHP_HTTP_DEFLATE_TYPE_RAW			0x00000020
#define PHP_HTTP_DEFLATE_STRATEGY_DEF		0x00000000
#define PHP_HTTP_DEFLATE_STRATEGY_FILT		0x00000100
#define PHP_HTTP_DEFLATE_STRATEGY_HUFF		0x00000200
#define PHP_HTTP_DEFLATE_STRATEGY_RLE		0x00000300
#define PHP_HTTP_DEFLATE_STRATEGY_FIXED		0x00000400

#define PHP_HTTP_DEFLATE_LEVEL_SET(flags, level) \
	switch (flags & 0xf) \
	{ \
		default: \
			if ((flags & 0xf) < 10) { \
				level = flags & 0xf; \
				break; \
			} \
		case PHP_HTTP_DEFLATE_LEVEL_DEF: \
			level = Z_DEFAULT_COMPRESSION; \
		break; \
	}
	
#define PHP_HTTP_DEFLATE_WBITS_SET(flags, wbits) \
	switch (flags & 0xf0) \
	{ \
		case PHP_HTTP_DEFLATE_TYPE_GZIP: \
			wbits = PHP_HTTP_WINDOW_BITS_GZIP; \
		break; \
		case PHP_HTTP_DEFLATE_TYPE_RAW: \
			wbits = PHP_HTTP_WINDOW_BITS_RAW; \
		break; \
		default: \
			wbits = PHP_HTTP_WINDOW_BITS_ZLIB; \
		break; \
	}

#define PHP_HTTP_INFLATE_WBITS_SET(flags, wbits) \
	if (flags & PHP_HTTP_INFLATE_TYPE_RAW) { \
		wbits = PHP_HTTP_WINDOW_BITS_RAW; \
} else { \
		wbits = PHP_HTTP_WINDOW_BITS_ANY; \
}

#define PHP_HTTP_DEFLATE_STRATEGY_SET(flags, strategy) \
	switch (flags & 0xf00) \
	{ \
		case PHP_HTTP_DEFLATE_STRATEGY_FILT: \
			strategy = Z_FILTERED; \
		break; \
		case PHP_HTTP_DEFLATE_STRATEGY_HUFF: \
			strategy = Z_HUFFMAN_ONLY; \
		break; \
		case PHP_HTTP_DEFLATE_STRATEGY_RLE: \
			strategy = Z_RLE; \
		break; \
		case PHP_HTTP_DEFLATE_STRATEGY_FIXED: \
			strategy = Z_FIXED; \
		break; \
		default: \
			strategy = Z_DEFAULT_STRATEGY; \
		break; \
	}

#define PHP_HTTP_WINDOW_BITS_ZLIB	0x0000000f
#define PHP_HTTP_WINDOW_BITS_GZIP	0x0000001f
#define PHP_HTTP_WINDOW_BITS_ANY	0x0000002f
#define PHP_HTTP_WINDOW_BITS_RAW	-0x000000f

#ifndef Z_FIXED
/* Z_FIXED does not exist prior 1.2.2.2 */
#	define Z_FIXED 0
#endif

#define PHP_HTTP_INFLATE_TYPE_ZLIB			0x00000000
#define PHP_HTTP_INFLATE_TYPE_GZIP			0x00000000
#define PHP_HTTP_INFLATE_TYPE_RAW			0x00000001

#define PHP_HTTP_ENCODING_STREAM_FLUSH_NONE	0x00000000
#define PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC 0x00100000
#define PHP_HTTP_ENCODING_STREAM_FLUSH_FULL 0x00200000

#define PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG(f) \
	(((f) & PHP_HTTP_ENCODING_STREAM_FLUSH_FULL) ? Z_FULL_FLUSH : \
	(((f) & PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC) ? Z_SYNC_FLUSH : Z_NO_FLUSH))

#define PHP_HTTP_ENCODING_STREAM_PERSISTENT	0x01000000

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

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_deflate_ops(void);
PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_inflate_ops(void);
PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_dechunk_ops(void);

PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_init(php_http_encoding_stream_t *s, php_http_encoding_stream_ops_t *ops, unsigned flags);
PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_reset(php_http_encoding_stream_t **s);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_update(php_http_encoding_stream_t *s, const char *in_str, size_t in_len, char **out_str, size_t *out_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_flush(php_http_encoding_stream_t *s, char **out_str, size_t *len);
PHP_HTTP_API zend_bool php_http_encoding_stream_done(php_http_encoding_stream_t *s);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_stream_finish(php_http_encoding_stream_t *s, char **out_str, size_t *len);
PHP_HTTP_API void php_http_encoding_stream_dtor(php_http_encoding_stream_t *s);
PHP_HTTP_API void php_http_encoding_stream_free(php_http_encoding_stream_t **s);

PHP_HTTP_API const char *php_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len);

typedef struct php_http_encoding_stream_object {
	php_http_encoding_stream_t *stream;
	zend_object zo;
} php_http_encoding_stream_object_t;

PHP_HTTP_API zend_class_entry *php_http_encoding_stream_class_entry;

zend_object *php_http_encoding_stream_object_new(zend_class_entry *ce);
php_http_encoding_stream_object_t *php_http_encoding_stream_object_new_ex(zend_class_entry *ce, php_http_encoding_stream_t *s);
zend_object *php_http_encoding_stream_object_clone(zval *object);
void php_http_encoding_stream_object_free(zend_object *object);

PHP_HTTP_API zend_class_entry *php_http_deflate_stream_class_entry;
PHP_HTTP_API zend_class_entry *php_http_inflate_stream_class_entry;
PHP_HTTP_API zend_class_entry *php_http_dechunk_stream_class_entry;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
