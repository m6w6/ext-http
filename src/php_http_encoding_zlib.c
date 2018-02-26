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

#include "php_http_api.h"

#define PHP_HTTP_INFLATE_ROUNDS				100
#define PHP_HTTP_INFLATE_BUFFER_SIZE		0x1000

#define PHP_HTTP_DEFLATE_BUFFER_SIZE		0x8000

#define PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(S) \
	(((size_t) ((double) S * (double) 1.015)) + 10 + 8 + 4 + 1)

#define PHP_HTTP_INFLATE_BUFFER_SIZE_GUESS(S) \
	(((S) + 1) << 3)
#define PHP_HTTP_INFLATE_BUFFER_SIZE_ALIGN(S) \
	((S) += (S) >> (3))

#define PHP_HTTP_INFLATE_FLUSH_FLAG(flags) \
	PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG((flags), Z_FULL_FLUSH, Z_SYNC_FLUSH, Z_NO_FLUSH)
#define PHP_HTTP_DEFLATE_FLUSH_FLAG(flags) \
	PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG((flags), Z_FULL_FLUSH, Z_SYNC_FLUSH, Z_NO_FLUSH)

#define PHP_HTTP_WINDOW_BITS_ZLIB			0x0000000f
#define PHP_HTTP_WINDOW_BITS_GZIP			0x0000001f
#define PHP_HTTP_WINDOW_BITS_ANY			0x0000002f
#define PHP_HTTP_WINDOW_BITS_RAW			-0x000000f

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

ZEND_RESULT_CODE php_http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len)
{
	int status, level, wbits, strategy;
	z_stream Z;

	PHP_HTTP_DEFLATE_LEVEL_SET(flags, level);
	PHP_HTTP_DEFLATE_WBITS_SET(flags, wbits);
	PHP_HTTP_DEFLATE_STRATEGY_SET(flags, strategy);

	memset(&Z, 0, sizeof(z_stream));
	*encoded = NULL;
	*encoded_len = 0;

	status = deflateInit2(&Z, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, strategy);
	if (EXPECTED(Z_OK == status)) {
		*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
		*encoded = emalloc(*encoded_len);

		Z.next_in = (Bytef *) data;
		Z.next_out = (Bytef *) *encoded;
		Z.avail_in = data_len;
		Z.avail_out = *encoded_len;

		status = deflate(&Z, Z_FINISH);
		deflateEnd(&Z);

		if (EXPECTED(Z_STREAM_END == status)) {
			/* size buffer down to actual length */
			*encoded = erealloc(*encoded, Z.total_out + 1);
			(*encoded)[*encoded_len = Z.total_out] = '\0';
			return SUCCESS;
		} else {
			PTR_SET(*encoded, NULL);
			*encoded_len = 0;
		}
	}

	php_error_docref(NULL, E_WARNING, "Could not deflate data: %s", zError(status));
	return FAILURE;
}

static php_http_encoding_stream_t *deflate_init(php_http_encoding_stream_t *s)
{
	int status, level, wbits, strategy, p = (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT);
	z_streamp ctx = pecalloc(1, sizeof(z_stream), p);

	PHP_HTTP_DEFLATE_LEVEL_SET(s->flags, level);
	PHP_HTTP_DEFLATE_WBITS_SET(s->flags, wbits);
	PHP_HTTP_DEFLATE_STRATEGY_SET(s->flags, strategy);

	if (EXPECTED(Z_OK == (status = deflateInit2(ctx, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, strategy)))) {
		if (EXPECTED(ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			s->ctx = ctx;
			return s;
		}
		deflateEnd(ctx);
		status = Z_MEM_ERROR;
	}
	pefree(ctx, p);
	php_error_docref(NULL, E_WARNING, "Failed to initialize deflate encoding stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *deflate_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int status, p = to->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	z_streamp from_ctx = from->ctx, to_ctx = pecalloc(1, sizeof(*to_ctx), p);

	if (Z_OK == (status = deflateCopy(to_ctx, from_ctx))) {
		if ((to_ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			php_http_buffer_append(to_ctx->opaque, PHP_HTTP_BUFFER(from_ctx->opaque)->data, PHP_HTTP_BUFFER(from_ctx->opaque)->used);
			to->ctx = to_ctx;
			return to;
		}
		deflateEnd(to_ctx);
		status = Z_MEM_ERROR;
	}
	php_error_docref(NULL, E_WARNING, "Failed to copy deflate encoding stream: %s", zError(status));
	return NULL;
}

static ZEND_RESULT_CODE deflate_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;

	/* append input to our buffer */
	php_http_buffer_append(PHP_HTTP_BUFFER(ctx->opaque), data, data_len);

	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;

	/* deflate */
	*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
	*encoded = emalloc(*encoded_len);
	ctx->avail_out = *encoded_len;
	ctx->next_out = (Bytef *) *encoded;

	switch (status = deflate(ctx, PHP_HTTP_DEFLATE_FLUSH_FLAG(s->flags))) {
		case Z_OK:
		case Z_STREAM_END:
			/* cut processed chunk off the buffer */
			if (ctx->avail_in) {
				php_http_buffer_cut(PHP_HTTP_BUFFER(ctx->opaque), 0, PHP_HTTP_BUFFER(ctx->opaque)->used - ctx->avail_in);
			} else {
				php_http_buffer_reset(PHP_HTTP_BUFFER(ctx->opaque));
			}

			/* size buffer down to actual size */
			*encoded_len -= ctx->avail_out;
			*encoded = erealloc(*encoded, *encoded_len + 1);
			(*encoded)[*encoded_len] = '\0';
			return SUCCESS;
	}

	PTR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_error_docref(NULL, E_WARNING, "Failed to update deflate stream: %s", zError(status));
	return FAILURE;
}

static ZEND_RESULT_CODE deflate_flush(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;

	*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE;
	*encoded = emalloc(*encoded_len);

	ctx->avail_in = 0;
	ctx->next_in = NULL;
	ctx->avail_out = *encoded_len;
	ctx->next_out = (Bytef *) *encoded;

	status = deflate(ctx, Z_FULL_FLUSH);
	if (EXPECTED(Z_OK == status || Z_STREAM_END == status)) {
		*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE - ctx->avail_out;
		*encoded = erealloc(*encoded, *encoded_len + 1);
		(*encoded)[*encoded_len] = '\0';
		return SUCCESS;
	}

	PTR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_error_docref(NULL, E_WARNING, "Failed to flush deflate stream: %s", zError(status));
	return FAILURE;
}

static ZEND_RESULT_CODE deflate_finish(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;

	*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE;
	*encoded = emalloc(*encoded_len);

	/* deflate remaining input */
	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;

	ctx->avail_out = *encoded_len;
	ctx->next_out = (Bytef *) *encoded;

	do {
		status = deflate(ctx, Z_FINISH);
	} while (Z_OK == status);

	if (EXPECTED(Z_STREAM_END == status)) {
		/* cut processed input off */
		php_http_buffer_cut(PHP_HTTP_BUFFER(ctx->opaque), 0, PHP_HTTP_BUFFER(ctx->opaque)->used - ctx->avail_in);

		/* size down */
		*encoded_len -= ctx->avail_out;
		*encoded = erealloc(*encoded, *encoded_len + 1);
		(*encoded)[*encoded_len] = '\0';
		return SUCCESS;
	}

	PTR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_error_docref(NULL, E_WARNING, "Failed to finish deflate stream: %s", zError(status));
	return FAILURE;
}

static zend_bool deflate_done(php_http_encoding_stream_t *s)
{
	z_streamp ctx = s->ctx;
	return !ctx->avail_in && !PHP_HTTP_BUFFER(ctx->opaque)->used;
}

static void deflate_dtor(php_http_encoding_stream_t *s)
{
	if (EXPECTED(s->ctx)) {
		z_streamp ctx = s->ctx;

		if (EXPECTED(ctx->opaque)) {
			php_http_buffer_free((php_http_buffer_t **) &ctx->opaque);
		}
		deflateEnd(ctx);
		pefree(ctx, (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		s->ctx = NULL;
	}
}

static inline int php_http_inflate_rounds(z_stream *Z, int flush, char **buf, size_t *len)
{
	int status = 0, round = 0;
	php_http_buffer_t buffer;

	*buf = NULL;
	*len = 0;

	php_http_buffer_init_ex(&buffer, Z->avail_in, PHP_HTTP_BUFFER_INIT_PREALLOC);

	do {
		if (UNEXPECTED(PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize_ex(&buffer, buffer.size, 0, 1))) {
			status = Z_MEM_ERROR;
		} else {
			Z->avail_out = buffer.free;
			Z->next_out = (Bytef *) buffer.data + buffer.used;
#if 0
			fprintf(stderr, "\n%3d: %3d PRIOR: size=%7lu,\tfree=%7lu,\tused=%7lu,\tavail_in=%7lu,\tavail_out=%7lu\n", round, status, buffer.size, buffer.free, buffer.used, Z->avail_in, Z->avail_out);
#endif
			status = inflate(Z, flush);
			php_http_buffer_account(&buffer, buffer.free - Z->avail_out);
#if 0
			fprintf(stderr, "%3d: %3d AFTER: size=%7lu,\tfree=%7lu,\tused=%7lu,\tavail_in=%7lu,\tavail_out=%7lu\n", round, status, buffer.size, buffer.free, buffer.used, Z->avail_in, Z->avail_out);
#endif
			PHP_HTTP_INFLATE_BUFFER_SIZE_ALIGN(buffer.size);
		}
	} while ((Z_BUF_ERROR == status || (Z_OK == status && Z->avail_in)) && ++round < PHP_HTTP_INFLATE_ROUNDS);

	if (EXPECTED(status == Z_OK || status == Z_STREAM_END)) {
		php_http_buffer_shrink(&buffer);
		php_http_buffer_fix(&buffer);
		*buf = buffer.data;
		*len = buffer.used;
	} else {
		php_http_buffer_dtor(&buffer);
	}

	return status;
}

ZEND_RESULT_CODE php_http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len)
{
	z_stream Z;
	int status, wbits = PHP_HTTP_WINDOW_BITS_ANY;

	memset(&Z, 0, sizeof(z_stream));

retry_raw_inflate:
	status = inflateInit2(&Z, wbits);
	if (EXPECTED(Z_OK == status)) {
		Z.next_in = (Bytef *) data;
		Z.avail_in = data_len + 1; /* include the terminating NULL, see #61287 */

		switch (status = php_http_inflate_rounds(&Z, Z_NO_FLUSH, decoded, decoded_len)) {
			case Z_STREAM_END:
				inflateEnd(&Z);
				return SUCCESS;

			case Z_OK:
				status = Z_DATA_ERROR;
				break;

			case Z_DATA_ERROR:
				/* raw deflated data? */
				if (PHP_HTTP_WINDOW_BITS_ANY == wbits) {
					inflateEnd(&Z);
					wbits = PHP_HTTP_WINDOW_BITS_RAW;
					goto retry_raw_inflate;
				}
				break;
		}
		inflateEnd(&Z);

		if (*decoded_len && *decoded) {
			efree(*decoded);
		}
	}

	php_error_docref(NULL, E_WARNING, "Could not inflate data: %s", zError(status));
	return FAILURE;
}

static php_http_encoding_stream_t *inflate_init(php_http_encoding_stream_t *s)
{
	int status, wbits, p = (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT);
	z_streamp ctx = pecalloc(1, sizeof(z_stream), p);

	PHP_HTTP_INFLATE_WBITS_SET(s->flags, wbits);

	if (EXPECTED(Z_OK == (status = inflateInit2(ctx, wbits)))) {
		if ((ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			s->ctx = ctx;
			return s;
		}
		inflateEnd(ctx);
		status = Z_MEM_ERROR;
	}
	pefree(ctx, p);
	php_error_docref(NULL, E_WARNING, "Failed to initialize inflate stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *inflate_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int status, p = from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	z_streamp from_ctx = from->ctx, to_ctx = pecalloc(1, sizeof(*to_ctx), p);

	if (Z_OK == (status = inflateCopy(to_ctx, from_ctx))) {
		if ((to_ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			php_http_buffer_append(to_ctx->opaque, PHP_HTTP_BUFFER(from_ctx->opaque)->data, PHP_HTTP_BUFFER(from_ctx->opaque)->used);
			to->ctx = to_ctx;
			return to;
		}
		inflateEnd(to_ctx);
		status = Z_MEM_ERROR;
	}
	php_error_docref(NULL, E_WARNING, "Failed to copy inflate encoding stream: %s", zError(status));
	return NULL;
}

static ZEND_RESULT_CODE inflate_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len)
{
	int status;
	z_streamp ctx = s->ctx;

	/* append input to buffer */
	php_http_buffer_append(PHP_HTTP_BUFFER(ctx->opaque), data, data_len);

retry_raw_inflate:
	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;

	switch (status = php_http_inflate_rounds(ctx, PHP_HTTP_INFLATE_FLUSH_FLAG(s->flags), decoded, decoded_len)) {
		case Z_OK:
		case Z_STREAM_END:
			/* cut off */
			if (ctx->avail_in) {
				php_http_buffer_cut(PHP_HTTP_BUFFER(ctx->opaque), 0, PHP_HTTP_BUFFER(ctx->opaque)->used - ctx->avail_in);
			} else {
				php_http_buffer_reset(PHP_HTTP_BUFFER(ctx->opaque));
			}
			return SUCCESS;

		case Z_DATA_ERROR:
			/* raw deflated data ? */
			if (!(s->flags & PHP_HTTP_INFLATE_TYPE_RAW) && !ctx->total_out) {
				inflateEnd(ctx);
				s->flags |= PHP_HTTP_INFLATE_TYPE_RAW;
				inflateInit2(ctx, PHP_HTTP_WINDOW_BITS_RAW);
				goto retry_raw_inflate;
			}
			break;
	}

	php_error_docref(NULL, E_WARNING, "Failed to update inflate stream: %s", zError(status));
	return FAILURE;
}

static ZEND_RESULT_CODE inflate_finish(php_http_encoding_stream_t *s, char **decoded, size_t *decoded_len)
{
	int status;
	z_streamp ctx = s->ctx;

	if (!PHP_HTTP_BUFFER(ctx->opaque)->used) {
		*decoded = NULL;
		*decoded_len = 0;
		return SUCCESS;
	}

	*decoded_len = (PHP_HTTP_BUFFER(ctx->opaque)->used + 1) * PHP_HTTP_INFLATE_ROUNDS;
	*decoded = emalloc(*decoded_len);

	/* inflate remaining input */
	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;

	ctx->avail_out = *decoded_len;
	ctx->next_out = (Bytef *) *decoded;

	if (Z_STREAM_END == (status = inflate(ctx, Z_FINISH))) {
		/* cut processed input off */
		php_http_buffer_cut(PHP_HTTP_BUFFER(ctx->opaque), 0, PHP_HTTP_BUFFER(ctx->opaque)->used - ctx->avail_in);

		/* size down */
		*decoded_len -= ctx->avail_out;
		*decoded = erealloc(*decoded, *decoded_len + 1);
		(*decoded)[*decoded_len] = '\0';
		return SUCCESS;
	}

	PTR_SET(*decoded, NULL);
	*decoded_len = 0;
	php_error_docref(NULL, E_WARNING, "Failed to finish inflate stream: %s", zError(status));
	return FAILURE;
}

static zend_bool inflate_done(php_http_encoding_stream_t *s)
{
	z_streamp ctx = s->ctx;
	return !ctx->avail_in && !PHP_HTTP_BUFFER(ctx->opaque)->used;
}

static void inflate_dtor(php_http_encoding_stream_t *s)
{
	if (EXPECTED(s->ctx)) {
		z_streamp ctx = s->ctx;

		if (EXPECTED(ctx->opaque)) {
			php_http_buffer_free((php_http_buffer_t **) &ctx->opaque);
		}
		inflateEnd(ctx);
		pefree(ctx, (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		s->ctx = NULL;
	}
}

static php_http_encoding_stream_ops_t php_http_encoding_deflate_ops = {
	deflate_init,
	deflate_copy,
	deflate_update,
	deflate_flush,
	deflate_done,
	deflate_finish,
	deflate_dtor
};

php_http_encoding_stream_ops_t *php_http_encoding_stream_get_deflate_ops(void)
{
	return &php_http_encoding_deflate_ops;
}

static php_http_encoding_stream_ops_t php_http_encoding_inflate_ops = {
	inflate_init,
	inflate_copy,
	inflate_update,
	NULL,
	inflate_done,
	inflate_finish,
	inflate_dtor
};

php_http_encoding_stream_ops_t *php_http_encoding_stream_get_inflate_ops(void)
{
	return &php_http_encoding_inflate_ops;
}

static zend_class_entry *php_http_deflate_stream_class_entry;
zend_class_entry *php_http_get_deflate_stream_class_entry(void)
{
	return php_http_deflate_stream_class_entry;
}

static zend_class_entry *php_http_inflate_stream_class_entry;
zend_class_entry *php_http_get_inflate_stream_class_entry(void)
{
	return php_http_inflate_stream_class_entry;
}


ZEND_BEGIN_ARG_INFO_EX(ai_HttpDeflateStream_encode, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpDeflateStream, encode)
{
	char *str;
	size_t len;
	zend_long flags = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &str, &len, &flags)) {
		char *enc_str = NULL;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_deflate(flags, str, len, &enc_str, &enc_len)) {
			if (enc_str) {
				RETURN_STR(php_http_cs2zs(enc_str, enc_len));
			} else {
				RETURN_EMPTY_STRING();
			}
		}
	}
	RETURN_FALSE;
}

static zend_function_entry php_http_deflate_stream_methods[] = {
	PHP_ME(HttpDeflateStream, encode, ai_HttpDeflateStream_encode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

ZEND_BEGIN_ARG_INFO_EX(ai_HttpInflateStream_decode, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpInflateStream, decode)
{
	char *str;
	size_t len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &len)) {
		char *enc_str = NULL;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_inflate(str, len, &enc_str, &enc_len)) {
			if (enc_str) {
				RETURN_STR(php_http_cs2zs(enc_str, enc_len));
			} else {
				RETURN_EMPTY_STRING();
			}
		}
	}
	RETURN_FALSE;
}

static zend_function_entry php_http_inflate_stream_methods[] = {
	PHP_ME(HttpInflateStream, decode, ai_HttpInflateStream_decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_encoding_zlib)
{
	zend_class_entry ce;

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding\\Stream", "Deflate", php_http_deflate_stream_methods);
	php_http_deflate_stream_class_entry = zend_register_internal_class_ex(&ce, php_http_get_encoding_stream_class_entry());
	php_http_deflate_stream_class_entry->create_object = php_http_encoding_stream_object_new;

	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_GZIP"), PHP_HTTP_DEFLATE_TYPE_GZIP);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_ZLIB"), PHP_HTTP_DEFLATE_TYPE_ZLIB);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_RAW"), PHP_HTTP_DEFLATE_TYPE_RAW);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_DEF"), PHP_HTTP_DEFLATE_LEVEL_DEF);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_MIN"), PHP_HTTP_DEFLATE_LEVEL_MIN);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_MAX"), PHP_HTTP_DEFLATE_LEVEL_MAX);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_DEF"), PHP_HTTP_DEFLATE_STRATEGY_DEF);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_FILT"), PHP_HTTP_DEFLATE_STRATEGY_FILT);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_HUFF"), PHP_HTTP_DEFLATE_STRATEGY_HUFF);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_RLE"), PHP_HTTP_DEFLATE_STRATEGY_RLE);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_FIXED"), PHP_HTTP_DEFLATE_STRATEGY_FIXED);

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding\\Stream", "Inflate", php_http_inflate_stream_methods);
	php_http_inflate_stream_class_entry = zend_register_internal_class_ex(&ce, php_http_get_encoding_stream_class_entry());
	php_http_inflate_stream_class_entry->create_object = php_http_encoding_stream_object_new;

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

