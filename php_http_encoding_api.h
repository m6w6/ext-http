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

#ifndef PHP_HTTP_ENCODING_API_H
#define PHP_HTTP_ENCODING_API_H

#define http_encoding_dechunk(e, el, d, dl) _http_encoding_dechunk((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_encoding_response_start(cl, i) _http_encoding_response_start((cl), (i) TSRMLS_CC)
PHP_HTTP_API int _http_encoding_response_start(size_t content_length, zend_bool ignore_http_ohandler TSRMLS_DC);

#ifdef HTTP_HAVE_ZLIB

extern PHP_MINIT_FUNCTION(http_encoding);
extern PHP_RINIT_FUNCTION(http_encoding);
extern PHP_RSHUTDOWN_FUNCTION(http_encoding);

typedef enum _http_encoding_type_t {
	HTTP_ENCODING_NONE,
	HTTP_ENCODING_GZIP,
	HTTP_ENCODING_DEFLATE,
} http_encoding_type;

#define HTTP_INFLATE_ROUNDS 100

#define HTTP_DEFLATE_BUFFER_SIZE_GUESS(S) \
	(((size_t) ((double) S * (double) 1.015)) + 10 + 8 + 4 + 1)
#define HTTP_INFLATE_BUFFER_SIZE_GUESS(S) \
	(((S) + 1) << 3)
#define HTTP_INFLATE_BUFFER_SIZE_ALIGN(S) \
	((S) += (S) >> (3))

#define HTTP_DEFLATE_BUFFER_SIZE		0x8000
#define HTTP_INFLATE_BUFFER_SIZE		0x1000

#define HTTP_DEFLATE_LEVEL_DEF			0x00000000
#define HTTP_DEFLATE_LEVEL_MIN			0x00000001
#define HTTP_DEFLATE_LEVEL_MAX			0x00000009
#define HTTP_DEFLATE_TYPE_ZLIB			0x00000000
#define HTTP_DEFLATE_TYPE_GZIP			0x00000010
#define HTTP_DEFLATE_TYPE_RAW			0x00000020
#define HTTP_DEFLATE_STRATEGY_DEF		0x00000000
#define HTTP_DEFLATE_STRATEGY_FILT		0x00000100
#define HTTP_DEFLATE_STRATEGY_HUFF		0x00000200
#define HTTP_DEFLATE_STRATEGY_RLE		0x00000300
#define HTTP_DEFLATE_STRATEGY_FIXED		0x00000400

#define HTTP_DEFLATE_LEVEL_SET(flags, level) \
	switch (flags & 0xf) \
	{ \
		default: \
			if ((flags & 0xf) < 10) { \
				level = flags & 0xf; \
				break; \
			} \
		case HTTP_DEFLATE_LEVEL_DEF: \
			level = Z_DEFAULT_COMPRESSION; \
		break; \
	}
	
#define HTTP_DEFLATE_WBITS_SET(flags, wbits) \
	switch (flags & 0xf0) \
	{ \
		case HTTP_DEFLATE_TYPE_GZIP: \
			wbits = HTTP_WINDOW_BITS_GZIP; \
		break; \
		case HTTP_DEFLATE_TYPE_RAW: \
			wbits = HTTP_WINDOW_BITS_RAW; \
		break; \
		default: \
			wbits = HTTP_WINDOW_BITS_ZLIB; \
		break; \
	}

#define HTTP_INFLATE_WBITS_SET(flags, wbits) \
	if (flags & HTTP_INFLATE_TYPE_RAW) { \
		wbits = HTTP_WINDOW_BITS_RAW; \
} else { \
		wbits = HTTP_WINDOW_BITS_ANY; \
}

#define HTTP_DEFLATE_STRATEGY_SET(flags, strategy) \
	switch (flags & 0xf00) \
	{ \
		case HTTP_DEFLATE_STRATEGY_FILT: \
			strategy = Z_FILTERED; \
		break; \
		case HTTP_DEFLATE_STRATEGY_HUFF: \
			strategy = Z_HUFFMAN_ONLY; \
		break; \
		case HTTP_DEFLATE_STRATEGY_RLE: \
			strategy = Z_RLE; \
		break; \
		case HTTP_DEFLATE_STRATEGY_FIXED: \
			strategy = Z_FIXED; \
		break; \
		default: \
			strategy = Z_DEFAULT_STRATEGY; \
		break; \
	}

#define HTTP_WINDOW_BITS_ZLIB	0x0000000f
#define HTTP_WINDOW_BITS_GZIP	0x0000001f
#define HTTP_WINDOW_BITS_ANY	0x0000002f
#define HTTP_WINDOW_BITS_RAW	-0x000000f

#ifndef Z_FIXED
/* Z_FIXED does not exist prior 1.2.2.2 */
#	define Z_FIXED 0
#endif

#define HTTP_INFLATE_TYPE_ZLIB			0x00000000
#define HTTP_INFLATE_TYPE_GZIP			0x00000000
#define HTTP_INFLATE_TYPE_RAW			0x00000001

#define HTTP_ENCODING_STREAM_FLUSH_NONE	0x00000000
#define HTTP_ENCODING_STREAM_FLUSH_SYNC 0x00100000
#define HTTP_ENCODING_STREAM_FLUSH_FULL 0x00200000

#define HTTP_ENCODING_STREAM_FLUSH_FLAG(f) \
	(((f) & HTTP_ENCODING_STREAM_FLUSH_FULL) ? Z_FULL_FLUSH : \
	(((f) & HTTP_ENCODING_STREAM_FLUSH_SYNC) ? Z_SYNC_FLUSH : Z_NO_FLUSH))

#define HTTP_ENCODING_STREAM_PERSISTENT	0x01000000

typedef struct _http_encoding_stream_t {
	z_stream stream;
	int flags;
	void *storage;
} http_encoding_stream;

#define http_encoding_deflate(f, d, dl, r, rl) _http_encoding_deflate((f), (d), (dl), (r), (rl) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_inflate(d, dl, r, rl) _http_encoding_inflate((d), (dl), (r), (rl) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);

#define http_encoding_deflate_stream_init(s, f) _http_encoding_deflate_stream_init((s), (f) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API http_encoding_stream *_http_encoding_deflate_stream_init(http_encoding_stream *s, int flags ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_deflate_stream_update(s, d, dl, e, el) _http_encoding_deflate_stream_update((s), (d), (dl), (e), (el) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_deflate_stream_flush(s, e, el) _http_encoding_deflate_stream_flush((s), (e), (el) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate_stream_flush(http_encoding_stream *s, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_deflate_stream_finish(s, e, el) _http_encoding_deflate_stream_finish((s), (e), (el) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate_stream_finish(http_encoding_stream *s, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_deflate_stream_dtor(s) _http_encoding_deflate_stream_dtor((s) TSRMLS_CC)
PHP_HTTP_API void _http_encoding_deflate_stream_dtor(http_encoding_stream *s TSRMLS_DC);
#define http_encoding_deflate_stream_free(s) _http_encoding_deflate_stream_free((s) TSRMLS_CC)
PHP_HTTP_API void _http_encoding_deflate_stream_free(http_encoding_stream **s TSRMLS_DC);

#define http_encoding_inflate_stream_init(s, f) _http_encoding_inflate_stream_init((s), (f) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API http_encoding_stream *_http_encoding_inflate_stream_init(http_encoding_stream *s, int flags ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_inflate_stream_update(s, d, dl, e, el) _http_encoding_inflate_stream_update((s), (d), (dl), (e), (el) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_inflate_stream_flush(s, d, dl) _http_encoding_inflate_stream_flush((s), (d), (dl) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate_stream_flush(http_encoding_stream *s, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_inflate_stream_finish(s, e, el) _http_encoding_inflate_stream_finish((s), (e), (el) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate_stream_finish(http_encoding_stream *s, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);
#define http_encoding_inflate_stream_dtor(s) _http_encoding_inflate_stream_dtor((s) TSRMLS_CC)
PHP_HTTP_API void _http_encoding_inflate_stream_dtor(http_encoding_stream *s TSRMLS_DC);
#define http_encoding_inflate_stream_free(s) _http_encoding_inflate_stream_free((s) TSRMLS_CC)
PHP_HTTP_API void _http_encoding_inflate_stream_free(http_encoding_stream **s TSRMLS_DC);

#define http_ob_deflatehandler(o, ol, h, hl, m) _http_ob_deflatehandler((o), (ol), (h), (hl), (m) TSRMLS_CC)
extern void _http_ob_deflatehandler(char *, uint, char **, uint *, int TSRMLS_DC);

#define http_ob_inflatehandler(o, ol, h, hl, m) _http_ob_inflatehandler((o), (ol), (h), (hl), (m) TSRMLS_CC)
extern void _http_ob_inflatehandler(char *, uint, char **, uint *, int TSRMLS_DC);

#endif /* HTTP_HAVE_ZLIB */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
