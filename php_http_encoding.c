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

#include "php_http_api.h"

#include <zlib.h>

static inline int eol_match(char **line, int *eol_len)
{
	char *ptr = *line;
	
	while (' ' == *ptr) ++ptr;

	if (ptr == php_http_locate_eol(*line, eol_len)) {
		*line = ptr;
		return 1;
	} else {
		return 0;
	}
}

PHP_HTTP_API const char *php_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int eol_len = 0;
	char *n_ptr = NULL;
	const char *e_ptr = encoded;
	
	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len + 1);

	while ((encoded + encoded_len - e_ptr) > 0) {
		ulong chunk_len = 0, rest;

		chunk_len = strtoul(e_ptr, &n_ptr, 16);

		/* we could not read in chunk size */
		if (n_ptr == e_ptr) {
			/*
			 * if this is the first turn and there doesn't seem to be a chunk
			 * size at the begining of the body, do not fail on apparently
			 * not encoded data and return a copy
			 */
			if (e_ptr == encoded) {
				php_http_error(HE_NOTICE, PHP_HTTP_E_ENCODING, "Data does not seem to be chunked encoded");
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				decoded[*decoded_len] = '\0';
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Expected chunk size at pos %tu of %zu but got trash", n_ptr - encoded, encoded_len);
				return NULL;
			}
		}
		
		/* reached the end */
		if (!chunk_len) {
			/* move over '0' chunked encoding terminator and any new lines */
			do {
				switch (*e_ptr) {
					case '0':
					case '\r':
					case '\n':
						++e_ptr;
						continue;
				}
			} while (0);
			break;
		}

		/* there should be CRLF after the chunk size, but we'll ignore SP+ too */
		if (*n_ptr && !eol_match(&n_ptr, &eol_len)) {
			if (eol_len == 2) {
				php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Expected CRLF at pos %tu of %zu but got 0x%02X 0x%02X", n_ptr - encoded, encoded_len, *n_ptr, *(n_ptr + 1));
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Expected LF at pos %tu of %zu but got 0x%02X", n_ptr - encoded, encoded_len, *n_ptr);
			}
		}
		n_ptr += eol_len;
		
		/* chunk size pretends more data than we actually got, so it's probably a truncated message */
		if (chunk_len > (rest = encoded + encoded_len - n_ptr)) {
			php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Truncated message: chunk size %lu exceeds remaining data size %lu at pos %tu of %zu", chunk_len, rest, n_ptr - encoded, encoded_len);
			chunk_len = rest;
		}

		/* copy the chunk */
		memcpy(*decoded + *decoded_len, n_ptr, chunk_len);
		*decoded_len += chunk_len;
		
		if (chunk_len == rest) {
			e_ptr = n_ptr + chunk_len;
			break;
		} else {
			/* advance to next chunk */
			e_ptr = n_ptr + chunk_len + eol_len;
		}
	}

	return e_ptr;
}

static inline int php_http_inflate_rounds(z_stream *Z, int flush, char **buf, size_t *len)
{
	int status = 0, round = 0;
	php_http_buffer_t buffer;
	
	*buf = NULL;
	*len = 0;
	
	php_http_buffer_init_ex(&buffer, Z->avail_in, PHP_HTTP_BUFFER_INIT_PREALLOC);
	
	do {
		if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_resize_ex(&buffer, buffer.size, 0, 1)) {
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
	
	if (status == Z_OK || status == Z_STREAM_END) {
		php_http_buffer_shrink(&buffer);
		php_http_buffer_fix(&buffer);
		*buf = buffer.data;
		*len = buffer.used;
	} else {
		php_http_buffer_dtor(&buffer);
	}
	
	return status;
}

PHP_HTTP_API STATUS php_http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
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
	if (Z_OK == status) {
		*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
		*encoded = emalloc(*encoded_len);
		
		Z.next_in = (Bytef *) data;
		Z.next_out = (Bytef *) *encoded;
		Z.avail_in = data_len;
		Z.avail_out = *encoded_len;
		
		status = deflate(&Z, Z_FINISH);
		deflateEnd(&Z);
		
		if (Z_STREAM_END == status) {
			/* size buffer down to actual length */
			*encoded = erealloc(*encoded, Z.total_out + 1);
			(*encoded)[*encoded_len = Z.total_out] = '\0';
			return SUCCESS;
		} else {
			STR_SET(*encoded, NULL);
			*encoded_len = 0;
		}
	}
	
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not deflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	z_stream Z;
	int status, wbits = PHP_HTTP_WINDOW_BITS_ANY;
	
	memset(&Z, 0, sizeof(z_stream));
	
retry_raw_inflate:
	status = inflateInit2(&Z, wbits);
	if (Z_OK == status) {
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

		if (decoded_len && *decoded) {
			efree(*decoded);
		}
	}
	
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not inflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_init(php_http_encoding_stream_t *s, php_http_encoding_stream_ops_t *ops, unsigned flags TSRMLS_DC)
{
	int freeme;

	if ((freeme = !s)) {
		s = pemalloc(sizeof(*s), (flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
	}
	memset(s, 0, sizeof(*s));

	s->flags = flags;
	TSRMLS_SET_CTX(s->ts);

	if ((s->ops = ops)) {
		php_http_encoding_stream_t *ss = s->ops->init(s);

		if (ss) {
			return ss;
		}
	} else {
		return s;
	}

	if (freeme) {
		pefree(s, (flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
	}
	return NULL;
}

PHP_HTTP_API php_http_encoding_stream_t *php_http_encoding_stream_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (from->ops->copy) {
		int freeme;
		php_http_encoding_stream_t *ns;

		if ((freeme = !to)) {
			to = pemalloc(sizeof(*to), (from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		}
		memset(to, 0, sizeof(*to));

		to->flags = from->flags;
		to->ops = from->ops;
		TSRMLS_SET_CTX(to->ts);

		if ((ns = to->ops->copy(from, to))) {
			return ns;
		} else {
			return to;
		}

		if (freeme) {
			pefree(to, (to->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		}
	}

	return NULL;
}

PHP_HTTP_API STATUS php_http_encoding_stream_reset(php_http_encoding_stream_t **s)
{
	php_http_encoding_stream_t *ss;
	if ((*s)->ops->dtor) {
		(*s)->ops->dtor(*s);
	}
	if ((ss = (*s)->ops->init(*s))) {
		*s = ss;
		return SUCCESS;
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_encoding_stream_update(php_http_encoding_stream_t *s, const char *in_str, size_t in_len, char **out_str, size_t *out_len)
{
	if (!s->ops->update) {
		return FAILURE;
	}
	return s->ops->update(s, in_str, in_len, out_str, out_len);
}

PHP_HTTP_API STATUS php_http_encoding_stream_flush(php_http_encoding_stream_t *s, char **out_str, size_t *out_len)
{
	if (!s->ops->flush) {
		*out_str = NULL;
		*out_len = 0;
		return SUCCESS;
	}
	return s->ops->flush(s, out_str, out_len);
}

PHP_HTTP_API zend_bool php_http_encoding_stream_done(php_http_encoding_stream_t *s)
{
	if (!s->ops->done) {
		return 0;
	}
	return s->ops->done(s);
}

PHP_HTTP_API STATUS php_http_encoding_stream_finish(php_http_encoding_stream_t *s, char **out_str, size_t *out_len)
{
	if (!s->ops->finish) {
		*out_str = NULL;
		*out_len = 0;
		return SUCCESS;
	}
	return s->ops->finish(s, out_str, out_len);
}

PHP_HTTP_API void php_http_encoding_stream_dtor(php_http_encoding_stream_t *s)
{
	if (s->ops->dtor) {
		s->ops->dtor(s);
	}
}

PHP_HTTP_API void php_http_encoding_stream_free(php_http_encoding_stream_t **s)
{
	if (*s) {
		if ((*s)->ops->dtor) {
			(*s)->ops->dtor(*s);
		}
		pefree(*s, ((*s)->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		*s = NULL;
	}
}

struct dechunk_ctx {
	php_http_buffer_t buffer;
	ulong hexlen;
	unsigned zeroed:1;
};

static php_http_encoding_stream_t *deflate_init(php_http_encoding_stream_t *s)
{
	int status, level, wbits, strategy, p = (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT);
	z_streamp ctx = pecalloc(1, sizeof(z_stream), p);
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
	PHP_HTTP_DEFLATE_LEVEL_SET(s->flags, level);
	PHP_HTTP_DEFLATE_WBITS_SET(s->flags, wbits);
	PHP_HTTP_DEFLATE_STRATEGY_SET(s->flags, strategy);
	
	if (Z_OK == (status = deflateInit2(ctx, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, strategy))) {
		if ((ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			s->ctx = ctx;
			return s;
		}
		deflateEnd(ctx);
		status = Z_MEM_ERROR;
	}
	pefree(ctx, p);
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to initialize deflate encoding stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *inflate_init(php_http_encoding_stream_t *s)
{
	int status, wbits, p = (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT);
	z_streamp ctx = pecalloc(1, sizeof(z_stream), p);
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
	PHP_HTTP_INFLATE_WBITS_SET(s->flags, wbits);
	
	if (Z_OK == (status = inflateInit2(ctx, wbits))) {
		if ((ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			s->ctx = ctx;
			return s;
		}
		inflateEnd(ctx);
		status = Z_MEM_ERROR;
	}
	pefree(ctx, p);
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to initialize inflate stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *dechunk_init(php_http_encoding_stream_t *s)
{
	struct dechunk_ctx *ctx = pecalloc(1, sizeof(*ctx), (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));

	if (!php_http_buffer_init_ex(&ctx->buffer, PHP_HTTP_BUFFER_DEFAULT_SIZE, (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT) ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0)) {
		return NULL;
	}

	ctx->hexlen = 0;
	ctx->zeroed = 0;
	s->ctx = ctx;

	return s;
}

static php_http_encoding_stream_t *deflate_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int status, p = to->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	z_streamp from_ctx = from->ctx, to_ctx = pecalloc(1, sizeof(*to_ctx), p);
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (Z_OK == (status = deflateCopy(to_ctx, from_ctx))) {
		if ((to_ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			php_http_buffer_append(to_ctx->opaque, PHP_HTTP_BUFFER(from_ctx->opaque)->data, PHP_HTTP_BUFFER(from_ctx->opaque)->used);
			to->ctx = to_ctx;
			return to;
		}
		deflateEnd(to_ctx);
		status = Z_MEM_ERROR;
	}
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to copy deflate encoding stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *inflate_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int status, p = from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	z_streamp from_ctx = from->ctx, to_ctx = pecalloc(1, sizeof(*to_ctx), p);
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (Z_OK == (status = inflateCopy(to_ctx, from_ctx))) {
		if ((to_ctx->opaque = php_http_buffer_init_ex(NULL, PHP_HTTP_DEFLATE_BUFFER_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0))) {
			php_http_buffer_append(to_ctx->opaque, PHP_HTTP_BUFFER(from_ctx->opaque)->data, PHP_HTTP_BUFFER(from_ctx->opaque)->used);
			to->ctx = to_ctx;
			return to;
		}
		inflateEnd(to_ctx);
		status = Z_MEM_ERROR;
	}
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to copy inflate encoding stream: %s", zError(status));
	return NULL;
}

static php_http_encoding_stream_t *dechunk_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int p = from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	struct dechunk_ctx *from_ctx = from->ctx, *to_ctx = pemalloc(sizeof(*to_ctx), p);
	TSRMLS_FETCH_FROM_CTX(from->ts);

	if (php_http_buffer_init_ex(&to_ctx->buffer, PHP_HTTP_BUFFER_DEFAULT_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0)) {
		to_ctx->hexlen = from_ctx->hexlen;
		to_ctx->zeroed = from_ctx->zeroed;
		php_http_buffer_append(&to_ctx->buffer, from_ctx->buffer.data, from_ctx->buffer.used);
		to->ctx = to_ctx;
		return to;
	}
	pefree(to_ctx, p);
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to copy inflate encoding stream: out of memory");
	return NULL;
}

static STATUS deflate_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
	/* append input to our buffer */
	php_http_buffer_append(PHP_HTTP_BUFFER(ctx->opaque), data, data_len);
	
	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;
	
	/* deflate */
	*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
	*encoded = emalloc(*encoded_len);
	ctx->avail_out = *encoded_len;
	ctx->next_out = (Bytef *) *encoded;
	
	switch (status = deflate(ctx, PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG(s->flags))) {
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
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to update deflate stream: %s", zError(status));
	return FAILURE;
}

static STATUS inflate_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len)
{
	int status;
	z_streamp ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
	/* append input to buffer */
	php_http_buffer_append(PHP_HTTP_BUFFER(ctx->opaque), data, data_len);

retry_raw_inflate:
	ctx->next_in = (Bytef *) PHP_HTTP_BUFFER(ctx->opaque)->data;
	ctx->avail_in = PHP_HTTP_BUFFER(ctx->opaque)->used;
	
	switch (status = php_http_inflate_rounds(ctx, PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG(s->flags), decoded, decoded_len)) {
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
	
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to update inflate stream: %s", zError(status));
	return FAILURE;
}

static STATUS dechunk_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len)
{
	php_http_buffer_t tmp;
	struct dechunk_ctx *ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);

	if (ctx->zeroed) {
		php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Dechunk encoding stream has already reached the end of chunked input");
		return FAILURE;
	}
	if ((PHP_HTTP_BUFFER_NOMEM == php_http_buffer_append(&ctx->buffer, data, data_len)) || !php_http_buffer_fix(&ctx->buffer)) {
		/* OOM */
		return FAILURE;
	}

	*decoded = NULL;
	*decoded_len = 0;

	php_http_buffer_init(&tmp);

	/* we have data in our buffer */
	while (ctx->buffer.used) {

		/* we already know the size of the chunk and are waiting for data */
		if (ctx->hexlen) {

			/* not enough data buffered */
			if (ctx->buffer.used < ctx->hexlen) {

				/* flush anyway? */
				if (s->flags & PHP_HTTP_ENCODING_STREAM_FLUSH_FULL) {
					/* flush all data (should only be chunk data) */
					php_http_buffer_append(&tmp, ctx->buffer.data, ctx->buffer.used);
					/* waiting for less data now */
					ctx->hexlen -= ctx->buffer.used;
					/* no more buffered data */
					php_http_buffer_reset(&ctx->buffer);
					/* break */
				}

				/* we have too less data and don't need to flush */
				else {
					break;
				}
			}

			/* we seem to have all data of the chunk */
			else {
				php_http_buffer_append(&tmp, ctx->buffer.data, ctx->hexlen);
				/* remove outgoing data from the buffer */
				php_http_buffer_cut(&ctx->buffer, 0, ctx->hexlen);
				/* reset hexlen */
				ctx->hexlen = 0;
				/* continue */
			}
		}

		/* we don't know the length of the chunk yet */
		else {
			size_t off = 0;

			/* ignore preceeding CRLFs (too loose?) */
			while (off < ctx->buffer.used && (
					ctx->buffer.data[off] == '\n' ||
					ctx->buffer.data[off] == '\r')) {
				++off;
			}
			if (off) {
				php_http_buffer_cut(&ctx->buffer, 0, off);
			}

			/* still data there? */
			if (ctx->buffer.used) {
				int eollen;
				const char *eolstr;

				/* we need eol, so we can be sure we have all hex digits */
				php_http_buffer_fix(&ctx->buffer);
				if ((eolstr = php_http_locate_bin_eol(ctx->buffer.data, ctx->buffer.used, &eollen))) {
					char *stop = NULL;

					/* read in chunk size */
					ctx->hexlen = strtoul(ctx->buffer.data, &stop, 16);

					/*	if strtoul() stops at the beginning of the buffered data
						there's something oddly wrong, i.e. bad input */
					if (stop == ctx->buffer.data) {
						php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to parse chunk len from '%.*s'", MIN(16, ctx->buffer.used), ctx->buffer.data);
						php_http_buffer_dtor(&tmp);
						return FAILURE;
					}

					/* cut out <chunk size hex><chunk extension><eol> */
					php_http_buffer_cut(&ctx->buffer, 0, eolstr + eollen - ctx->buffer.data);
					/* buffer->hexlen is 0 now or contains the size of the next chunk */
					if (!ctx->hexlen) {
						size_t off = 0;

						/* ignore following CRLFs (too loose?) */
						while (off < ctx->buffer.used && (
								ctx->buffer.data[off] == '\n' ||
								ctx->buffer.data[off] == '\r')) {
							++off;
						}
						if (off) {
							php_http_buffer_cut(&ctx->buffer, 0, off);
						}

						ctx->zeroed = 1;
						break;
					}
					/* continue */
				} else {
					/* we have not enough data buffered to read in chunk size */
					break;
				}
			}
			/* break */
		}
	}

	php_http_buffer_fix(&tmp);
	*decoded = tmp.data;
	*decoded_len = tmp.used;

	return SUCCESS;
}

static STATUS deflate_flush(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
	*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE;
	*encoded = emalloc(*encoded_len);
	
	ctx->avail_in = 0;
	ctx->next_in = NULL;
	ctx->avail_out = *encoded_len;
	ctx->next_out = (Bytef *) *encoded;
	
	switch (status = deflate(ctx, Z_FULL_FLUSH)) {
		case Z_OK:
		case Z_STREAM_END:
			*encoded_len = PHP_HTTP_DEFLATE_BUFFER_SIZE - ctx->avail_out;
			*encoded = erealloc(*encoded, *encoded_len + 1);
			(*encoded)[*encoded_len] = '\0';
			return SUCCESS;
	}
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to flush deflate stream: %s", zError(status));
	return FAILURE;
}

static STATUS dechunk_flush(php_http_encoding_stream_t *s, char **decoded, size_t *decoded_len)
{
	struct dechunk_ctx *ctx = s->ctx;

	if (ctx->hexlen) {
		/* flush all data (should only be chunk data) */
		php_http_buffer_fix(&ctx->buffer);
		php_http_buffer_data(&ctx->buffer, decoded, decoded_len);
		/* waiting for less data now */
		ctx->hexlen -= ctx->buffer.used;
		/* no more buffered data */
		php_http_buffer_reset(&ctx->buffer);
	} else {
		*decoded = NULL;
		*decoded_len = 0;
	}

	return SUCCESS;
}

static STATUS deflate_finish(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	int status;
	z_streamp ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
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
	
	if (Z_STREAM_END == status) {
		/* cut processed input off */
		php_http_buffer_cut(PHP_HTTP_BUFFER(ctx->opaque), 0, PHP_HTTP_BUFFER(ctx->opaque)->used - ctx->avail_in);
		
		/* size down */
		*encoded_len -= ctx->avail_out;
		*encoded = erealloc(*encoded, *encoded_len + 1);
		(*encoded)[*encoded_len] = '\0';
		return SUCCESS;
	}
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to finish deflate stream: %s", zError(status));
	return FAILURE;
}

static STATUS inflate_finish(php_http_encoding_stream_t *s, char **decoded, size_t *decoded_len)
{
	int status;
	z_streamp ctx = s->ctx;
	TSRMLS_FETCH_FROM_CTX(s->ts);
	
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
	
	STR_SET(*decoded, NULL);
	*decoded_len = 0;
	php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Failed to finish inflate stream: %s", zError(status));
	return FAILURE;
}

static zend_bool deflate_done(php_http_encoding_stream_t *s)
{
	z_streamp ctx = s->ctx;
	return !ctx->avail_in && !PHP_HTTP_BUFFER(ctx->opaque)->used;
}

static zend_bool inflate_done(php_http_encoding_stream_t *s)
{
	z_streamp ctx = s->ctx;
	return !ctx->avail_in && !PHP_HTTP_BUFFER(ctx->opaque)->used;
}

static zend_bool dechunk_done(php_http_encoding_stream_t *s)
{
	return ((struct dechunk_ctx *) s->ctx)->zeroed;
}

static void deflate_dtor(php_http_encoding_stream_t *s)
{
	if (s->ctx) {
		z_streamp ctx = s->ctx;

		if (ctx->opaque) {
			php_http_buffer_free((php_http_buffer_t **) &ctx->opaque);
		}
		deflateEnd(ctx);
		pefree(ctx, (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		s->ctx = NULL;
	}
}

static void inflate_dtor(php_http_encoding_stream_t *s)
{
	if (s->ctx) {
		z_streamp ctx = s->ctx;

		if (ctx->opaque) {
			php_http_buffer_free((php_http_buffer_t **) &ctx->opaque);
		}
		inflateEnd(ctx);
		pefree(ctx, (s->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		s->ctx = NULL;
	}
}

static void dechunk_dtor(php_http_encoding_stream_t *s)
{
	if (s->ctx) {
		struct dechunk_ctx *ctx = s->ctx;

		php_http_buffer_dtor(&ctx->buffer);
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

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_deflate_ops(void)
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

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_inflate_ops(void)
{
	return &php_http_encoding_inflate_ops;
}

static php_http_encoding_stream_ops_t php_http_encoding_dechunk_ops = {
	dechunk_init,
	dechunk_copy,
	dechunk_update,
	dechunk_flush,
	dechunk_done,
	NULL,
	dechunk_dtor
};

PHP_HTTP_API php_http_encoding_stream_ops_t *php_http_encoding_stream_get_dechunk_ops(void)
{
	return &php_http_encoding_dechunk_ops;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpEncodingStream, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpEncodingStream, method, 0)
#define PHP_HTTP_ENCSTREAM_ME(method, visibility)	PHP_ME(HttpEncodingStream, method, PHP_HTTP_ARGS(HttpEncodingStream, method), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(update, 1)
	PHP_HTTP_ARG_VAL(data, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(flush);
PHP_HTTP_EMPTY_ARGS(done);
PHP_HTTP_EMPTY_ARGS(finish);

static zend_class_entry *php_http_encoding_stream_class_entry;

zend_class_entry *php_http_encoding_stream_get_class_entry(void)
{
	return php_http_encoding_stream_class_entry;
}

static zend_function_entry php_http_encoding_stream_method_entry[] = {
	PHP_HTTP_ENCSTREAM_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_ENCSTREAM_ME(update, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENCSTREAM_ME(flush, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENCSTREAM_ME(done, ZEND_ACC_PUBLIC)
	PHP_HTTP_ENCSTREAM_ME(finish, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_encoding_stream_object_handlers;

zend_object_value php_http_encoding_stream_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_encoding_stream_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_encoding_stream_object_new_ex(zend_class_entry *ce, php_http_encoding_stream_t *s, php_http_encoding_stream_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_encoding_stream_object_t *o;

	o = ecalloc(1, sizeof(*o));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	if (s) {
		o->stream = s;
	}

	ov.handle = zend_objects_store_put((zend_object *) o, NULL, php_http_encoding_stream_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_encoding_stream_object_handlers;

	return ov;
}

zend_object_value php_http_encoding_stream_object_clone(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_encoding_stream_object_t *new_obj = NULL, *old_obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	new_ov = php_http_encoding_stream_object_new_ex(old_obj->zo.ce, php_http_encoding_stream_copy(old_obj->stream, NULL), &new_obj TSRMLS_CC);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);

	return new_ov;
}

void php_http_encoding_stream_object_free(void *object TSRMLS_DC)
{
	php_http_encoding_stream_object_t *o = (php_http_encoding_stream_object_t *) object;

	if (o->stream) {
		php_http_encoding_stream_free(&o->stream);
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

PHP_METHOD(HttpEncodingStream, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		long flags = 0;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags)) {
			with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
				php_http_encoding_stream_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				if (!obj->stream) {
					php_http_encoding_stream_ops_t *ops = NULL;

					if (instanceof_function(obj->zo.ce, php_http_deflate_stream_get_class_entry() TSRMLS_CC)) {
						ops = &php_http_encoding_deflate_ops;
					} else if (instanceof_function(obj->zo.ce, php_http_inflate_stream_get_class_entry() TSRMLS_CC)) {
						ops = &php_http_encoding_inflate_ops;
					} else if (instanceof_function(obj->zo.ce, php_http_dechunk_stream_get_class_entry() TSRMLS_CC)) {
						ops = &php_http_encoding_dechunk_ops;
					}

					if (ops) {
						obj->stream = php_http_encoding_stream_init(obj->stream, ops, flags TSRMLS_CC);
					} else {
						php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Unknown HttpEncodingStream class %s", obj->zo.ce->name);
					}

				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "HttpEncodingStream cannot be initialized twice");
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpEncodingStream, update)
{
	int data_len;
	char *data_str;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data_str, &data_len)) {
		php_http_encoding_stream_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->stream) {
			size_t encoded_len;
			char *encoded_str;

			if (SUCCESS == php_http_encoding_stream_update(obj->stream, data_str, data_len, &encoded_str, &encoded_len)) {
				RETURN_STRINGL(encoded_str, encoded_len, 0);
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEncodingStream, flush)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_encoding_stream_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->stream) {
			char *encoded_str;
			size_t encoded_len;

			if (SUCCESS == php_http_encoding_stream_flush(obj->stream, &encoded_str, &encoded_len)) {
				if (encoded_str) {
					RETURN_STRINGL(encoded_str, encoded_len, 0);
				} else {
					RETURN_EMPTY_STRING();
				}
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEncodingStream, done)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_encoding_stream_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->stream) {
			RETURN_BOOL(php_http_encoding_stream_done(obj->stream));
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEncodingStream, finish)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_encoding_stream_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->stream) {
			char *encoded_str;
			size_t encoded_len;

			if (SUCCESS == php_http_encoding_stream_finish(obj->stream, &encoded_str, &encoded_len)) {
				if (SUCCESS == php_http_encoding_stream_reset(&obj->stream)) {
					if (encoded_str) {
						RETURN_STRINGL(encoded_str, encoded_len, 0);
					} else {
						RETURN_EMPTY_STRING();
					}
				} else {
					STR_FREE(encoded_str);
				}
			}
		}
	}
	RETURN_FALSE;
}

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpDeflateStream, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpDeflateStream, method, 0)
#define PHP_HTTP_DEFLATE_ME(method, visibility)	PHP_ME(HttpDeflateStream, method, PHP_HTTP_ARGS(HttpDeflateStream, method), visibility)

PHP_HTTP_BEGIN_ARGS(encode, 1)
	PHP_HTTP_ARG_VAL(data, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_deflate_stream_class_entry;

zend_class_entry *php_http_deflate_stream_get_class_entry(void)
{
	return php_http_deflate_stream_class_entry;
}

static zend_function_entry php_http_deflate_stream_method_entry[] = {
	PHP_HTTP_DEFLATE_ME(encode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpDeflateStream, encode)
{
	char *str;
	int len;
	long flags = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &str, &len, &flags)) {
		char *enc_str;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_deflate(flags, str, len, &enc_str, &enc_len TSRMLS_CC)) {
			RETURN_STRINGL(enc_str, enc_len, 0);
		}
	}
	RETURN_FALSE;
}

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpInflateStream, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpInflateStream, method, 0)
#define PHP_HTTP_INFLATE_ME(method, visibility)	PHP_ME(HttpInflateStream, method, PHP_HTTP_ARGS(HttpInflateStream, method), visibility)

PHP_HTTP_BEGIN_ARGS(decode, 1)
	PHP_HTTP_ARG_VAL(data, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_inflate_stream_class_entry;

zend_class_entry *php_http_inflate_stream_get_class_entry(void)
{
	return php_http_inflate_stream_class_entry;
}

static zend_function_entry php_http_inflate_stream_method_entry[] = {
	PHP_HTTP_INFLATE_ME(decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpInflateStream, decode)
{
	char *str;
	int len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len)) {
		char *enc_str;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_inflate(str, len, &enc_str, &enc_len TSRMLS_CC)) {
			RETURN_STRINGL(enc_str, enc_len, 0);
		}
	}
	RETURN_FALSE;
}

#undef PHP_HTTP_BEGIN_ARGS
#undef PHP_HTTP_EMPTY_ARGS
#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpDechunkStream, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpDechunkStream, method, 0)
#define PHP_HTTP_DECHUNK_ME(method, visibility)	PHP_ME(HttpDechunkStream, method, PHP_HTTP_ARGS(HttpDechunkStream, method), visibility)

PHP_HTTP_BEGIN_ARGS(decode, 1)
	PHP_HTTP_ARG_VAL(data, 0)
	PHP_HTTP_ARG_VAL(decoded_len, 1)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_dechunk_stream_class_entry;

zend_class_entry *php_http_dechunk_stream_get_class_entry(void)
{
	return php_http_dechunk_stream_class_entry;
}

static zend_function_entry php_http_dechunk_stream_method_entry[] = {
	PHP_HTTP_DECHUNK_ME(decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpDechunkStream, decode)
{
	char *str;
	int len;
	zval *zlen = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z!", &str, &len, &zlen)) {
		const char *end_ptr;
		char *enc_str;
		size_t enc_len;

		if ((end_ptr = php_http_encoding_dechunk(str, len, &enc_str, &enc_len TSRMLS_CC))) {
			if (zlen) {
				zval_dtor(zlen);
				ZVAL_LONG(zlen, str + len - end_ptr);
			}
			RETURN_STRINGL(enc_str, enc_len, 0);
		}
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_encoding)
{
	PHP_HTTP_REGISTER_CLASS(http\\Encoding, Stream, http_encoding_stream, php_http_object_get_class_entry(), ZEND_ACC_EXPLICIT_ABSTRACT_CLASS);
	php_http_encoding_stream_class_entry->create_object = php_http_encoding_stream_object_new;
	memcpy(&php_http_encoding_stream_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_encoding_stream_object_handlers.clone_obj = php_http_encoding_stream_object_clone;

	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_NONE"), PHP_HTTP_ENCODING_STREAM_FLUSH_NONE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_SYNC"), PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC TSRMLS_CC);
	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_FULL"), PHP_HTTP_ENCODING_STREAM_FLUSH_FULL TSRMLS_CC);

	PHP_HTTP_REGISTER_CLASS(http\\Encoding\\Stream, Deflate, http_deflate_stream, php_http_encoding_stream_class_entry, 0);

	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_GZIP"), PHP_HTTP_DEFLATE_TYPE_GZIP TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_ZLIB"), PHP_HTTP_DEFLATE_TYPE_ZLIB TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("TYPE_RAW"), PHP_HTTP_DEFLATE_TYPE_RAW TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_DEF"), PHP_HTTP_DEFLATE_LEVEL_DEF TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_MIN"), PHP_HTTP_DEFLATE_LEVEL_MIN TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("LEVEL_MAX"), PHP_HTTP_DEFLATE_LEVEL_MAX TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_DEF"), PHP_HTTP_DEFLATE_STRATEGY_DEF TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_FILT"), PHP_HTTP_DEFLATE_STRATEGY_FILT TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_HUFF"), PHP_HTTP_DEFLATE_STRATEGY_HUFF TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_RLE"), PHP_HTTP_DEFLATE_STRATEGY_RLE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_deflate_stream_class_entry, ZEND_STRL("STRATEGY_FIXED"), PHP_HTTP_DEFLATE_STRATEGY_FIXED TSRMLS_CC);

	PHP_HTTP_REGISTER_CLASS(http\\Encoding\\Stream, Inflate, http_inflate_stream, php_http_encoding_stream_class_entry, 0);
	PHP_HTTP_REGISTER_CLASS(http\\Encoding\\Stream, Dechunk, http_dechunk_stream, php_http_encoding_stream_class_entry, 0);

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

