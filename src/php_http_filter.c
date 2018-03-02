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

#ifndef DBG_FILTER
#	define DBG_FILTER 0
#endif

PHP_MINIT_FUNCTION(http_filter)
{
	php_stream_filter_register_factory("http.*", &php_http_filter_factory);
	return SUCCESS;
}

#define PHP_HTTP_FILTER_PARAMS \
	php_stream *stream, \
	php_stream_filter *this, \
	php_stream_bucket_brigade *buckets_in, \
	php_stream_bucket_brigade *buckets_out, \
	size_t *bytes_consumed, \
	int flags
#define PHP_HTTP_FILTER_OP(filter) \
	http_filter_op_ ##filter
#define PHP_HTTP_FILTER_OPS(filter) \
	php_stream_filter_ops PHP_HTTP_FILTER_OP(filter)
#define PHP_HTTP_FILTER_DTOR(filter) \
	http_filter_ ##filter## _dtor
#define PHP_HTTP_FILTER_DESTRUCTOR(filter) \
	void PHP_HTTP_FILTER_DTOR(filter)(php_stream_filter *this)
#define PHP_HTTP_FILTER_FUNC(filter) \
	http_filter_ ##filter
#define PHP_HTTP_FILTER_FUNCTION(filter) \
	php_stream_filter_status_t PHP_HTTP_FILTER_FUNC(filter)(PHP_HTTP_FILTER_PARAMS)
#define PHP_HTTP_FILTER_BUFFER(filter) \
	http_filter_ ##filter## _buffer

#define PHP_HTTP_FILTER_IS_CLOSING(stream, flags) \
	(	(flags & PSFS_FLAG_FLUSH_CLOSE) \
	||	php_stream_eof(stream) \
	|| ((stream->ops == &php_stream_temp_ops || stream->ops == &php_stream_memory_ops) && stream->eof) \
	)

#define NEW_BUCKET(data, length) \
	{ \
		char *__data; \
		php_stream_bucket *__buck; \
		\
		__data = pemalloc(length, this->is_persistent); \
		if (!__data) { \
			return PSFS_ERR_FATAL; \
		} \
		memcpy(__data, data, length); \
		\
		__buck = php_stream_bucket_new(stream, __data, length, 1, this->is_persistent); \
		if (!__buck) { \
			pefree(__data, this->is_persistent); \
			return PSFS_ERR_FATAL; \
		} \
		\
		php_stream_bucket_append(buckets_out, __buck); \
	}

typedef struct _http_chunked_decode_filter_buffer_t {
	php_http_buffer_t	buffer;
	ulong	hexlen;
} PHP_HTTP_FILTER_BUFFER(chunked_decode);

typedef php_http_encoding_stream_t PHP_HTTP_FILTER_BUFFER(stream);

static PHP_HTTP_FILTER_FUNCTION(chunked_decode)
{
	int out_avail = 0;
	php_stream_bucket *ptr, *nxt;
	PHP_HTTP_FILTER_BUFFER(chunked_decode) *buffer = Z_PTR(this->abstract);
	
	if (bytes_consumed) {
		*bytes_consumed = 0;
	}
	
	/* fetch available bucket data */
	for (ptr = buckets_in->head; ptr; ptr = nxt) {
		if (bytes_consumed) {
			*bytes_consumed += ptr->buflen;
		}

		if (PHP_HTTP_BUFFER_NOMEM == php_http_buffer_append(PHP_HTTP_BUFFER(buffer), ptr->buf, ptr->buflen)) {
			return PSFS_ERR_FATAL;
		}

		nxt = ptr->next;
		php_stream_bucket_unlink(ptr);
		php_stream_bucket_delref(ptr);
	}
	
	if (!php_http_buffer_fix(PHP_HTTP_BUFFER(buffer))) {
		return PSFS_ERR_FATAL;
	}

	/* we have data in our buffer */
	while (PHP_HTTP_BUFFER(buffer)->used) {
	
		/* we already know the size of the chunk and are waiting for data */
		if (buffer->hexlen) {
		
			/* not enough data buffered */
			if (PHP_HTTP_BUFFER(buffer)->used < buffer->hexlen) {
			
				/* flush anyway? */
				if (flags & PSFS_FLAG_FLUSH_INC) {
				
					/* flush all data (should only be chunk data) */
					out_avail = 1;
					NEW_BUCKET(PHP_HTTP_BUFFER(buffer)->data, PHP_HTTP_BUFFER(buffer)->used);
					
					/* waiting for less data now */
					buffer->hexlen -= PHP_HTTP_BUFFER(buffer)->used;
					/* no more buffered data */
					php_http_buffer_reset(PHP_HTTP_BUFFER(buffer));
					/* break */
				} 
				
				/* we have too less data and don't need to flush */
				else {
					break;
				}
			} 
			
			/* we seem to have all data of the chunk */
			else {
				out_avail = 1;
				NEW_BUCKET(PHP_HTTP_BUFFER(buffer)->data, buffer->hexlen);
				
				/* remove outgoing data from the buffer */
				php_http_buffer_cut(PHP_HTTP_BUFFER(buffer), 0, buffer->hexlen);
				/* reset hexlen */
				buffer->hexlen = 0;
				/* continue */
			}
		} 
		
		/* we don't know the length of the chunk yet */
		else {
			size_t off = 0;
			
			/* ignore preceeding CRLFs (too loose?) */
			while (off < PHP_HTTP_BUFFER(buffer)->used && (
					PHP_HTTP_BUFFER(buffer)->data[off] == '\n' || 
					PHP_HTTP_BUFFER(buffer)->data[off] == '\r')) {
				++off;
			}
			if (off) {
				php_http_buffer_cut(PHP_HTTP_BUFFER(buffer), 0, off);
			}
			
			/* still data there? */
			if (PHP_HTTP_BUFFER(buffer)->used) {
				int eollen;
				const char *eolstr;
				
				/* we need eol, so we can be sure we have all hex digits */
				php_http_buffer_fix(PHP_HTTP_BUFFER(buffer));
				if ((eolstr = php_http_locate_bin_eol(PHP_HTTP_BUFFER(buffer)->data, PHP_HTTP_BUFFER(buffer)->used, &eollen))) {
					char *stop = NULL;
					
					/* read in chunk size */
					buffer->hexlen = strtoul(PHP_HTTP_BUFFER(buffer)->data, &stop, 16);
					
					/*	if strtoul() stops at the beginning of the buffered data
						there's something oddly wrong, i.e. bad input */
					if (stop == PHP_HTTP_BUFFER(buffer)->data) {
						return PSFS_ERR_FATAL;
					}
					
					/* cut out <chunk size hex><chunk extension><eol> */
					php_http_buffer_cut(PHP_HTTP_BUFFER(buffer), 0, eolstr + eollen - PHP_HTTP_BUFFER(buffer)->data);
					/* buffer->hexlen is 0 now or contains the size of the next chunk */
					if (!buffer->hexlen) {
						php_stream_notify_info(PHP_STREAM_CONTEXT(stream), PHP_STREAM_NOTIFY_COMPLETED, NULL, 0);
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
	
	/* flush before close, but only if we are already waiting for more data */
	if (PHP_HTTP_FILTER_IS_CLOSING(stream, flags) && buffer->hexlen && PHP_HTTP_BUFFER(buffer)->used) {
		out_avail = 1;
		NEW_BUCKET(PHP_HTTP_BUFFER(buffer)->data, PHP_HTTP_BUFFER(buffer)->used);
		php_http_buffer_reset(PHP_HTTP_BUFFER(buffer));
		buffer->hexlen = 0;
	}
	
	return out_avail ? PSFS_PASS_ON : PSFS_FEED_ME;
}

static PHP_HTTP_FILTER_DESTRUCTOR(chunked_decode)
{
	PHP_HTTP_FILTER_BUFFER(chunked_decode) *b = Z_PTR(this->abstract);
	
	php_http_buffer_dtor(PHP_HTTP_BUFFER(b));
	pefree(b, this->is_persistent);
}

static PHP_HTTP_FILTER_FUNCTION(chunked_encode)
{
	php_http_buffer_t buf;
	php_stream_bucket *ptr, *nxt;
	
	if (bytes_consumed) {
		*bytes_consumed = 0;
	}
	
	/* new data available? */
	php_http_buffer_init(&buf);

	/* fetch available bucket data */
	for (ptr = buckets_in->head; ptr; ptr = nxt) {
		if (bytes_consumed) {
			*bytes_consumed += ptr->buflen;
		}
#if DBG_FILTER
		fprintf(stderr, "update: chunked (-> %zu) (w: %zu, r: %zu)\n", ptr->buflen, stream->writepos, stream->readpos);
#endif
		
		nxt = ptr->next;
		php_stream_bucket_unlink(ptr);
		php_http_buffer_appendf(&buf, "%lx" PHP_HTTP_CRLF, (long unsigned int) ptr->buflen);
		php_http_buffer_append(&buf, ptr->buf, ptr->buflen);
		php_http_buffer_appends(&buf, PHP_HTTP_CRLF);

		/* pass through */
		NEW_BUCKET(buf.data, buf.used);
		/* reset */
		php_http_buffer_reset(&buf);
		php_stream_bucket_delref(ptr);
	}

	/* free buffer */
	php_http_buffer_dtor(&buf);
	
	/* terminate with "0" */
	if (PHP_HTTP_FILTER_IS_CLOSING(stream, flags)) {
#if DBG_FILTER
		fprintf(stderr, "finish: chunked\n");
#endif
		
		NEW_BUCKET("0" PHP_HTTP_CRLF PHP_HTTP_CRLF, lenof("0" PHP_HTTP_CRLF PHP_HTTP_CRLF));
	}
	
	return PSFS_PASS_ON;
}

static PHP_HTTP_FILTER_FUNCTION(stream)
{
	php_stream_bucket *ptr, *nxt;
	PHP_HTTP_FILTER_BUFFER(stream) *buffer = Z_PTR(this->abstract);
	
	if (bytes_consumed) {
		*bytes_consumed = 0;
	}
	
	/* fetch available bucket data */
	for (ptr = buckets_in->head; ptr; ptr = nxt) {
		char *encoded = NULL;
		size_t encoded_len = 0;

		if (bytes_consumed) {
			*bytes_consumed += ptr->buflen;
		}

#if DBG_FILTER
		fprintf(stderr, "bucket: b=%p p=%p p=%p\n", ptr->brigade, ptr->prev,  ptr->next);
#endif
		
		nxt = ptr->next;
		php_stream_bucket_unlink(ptr);
		if (SUCCESS != php_http_encoding_stream_update(buffer, ptr->buf, ptr->buflen, &encoded, &encoded_len)) {
			return PSFS_ERR_FATAL;
		}
		
#if DBG_FILTER
		fprintf(stderr, "update: compress (-> %zu) (w: %zu, r: %zu)\n", encoded_len, stream->writepos, stream->readpos);
#endif
		
		if (encoded) {
			if (encoded_len) {
				NEW_BUCKET(encoded, encoded_len);
			}
			efree(encoded);
		}
		php_stream_bucket_delref(ptr);
	}

	/* flush & close */
	if (flags & PSFS_FLAG_FLUSH_INC) {
		char *encoded = NULL;
		size_t encoded_len = 0;
		
		if (SUCCESS != php_http_encoding_stream_flush(buffer, &encoded, &encoded_len)) {
			return PSFS_ERR_FATAL;
		}
		
#if DBG_FILTER
		fprintf(stderr, "flush: compress (-> %zu)\n", encoded_len);
#endif
		
		if (encoded) {
			if (encoded_len) {
				NEW_BUCKET(encoded, encoded_len);
			}
			efree(encoded);
		}
	}
	
	if (PHP_HTTP_FILTER_IS_CLOSING(stream, flags)) {
		char *encoded = NULL;
		size_t encoded_len = 0;
		
		if (SUCCESS != php_http_encoding_stream_finish(buffer, &encoded, &encoded_len)) {
			return PSFS_ERR_FATAL;
		}
		
#if DBG_FILTER
		fprintf(stderr, "finish: compress (-> %zu)\n", encoded_len);
#endif
		
		if (encoded) {
			if (encoded_len) {
				NEW_BUCKET(encoded, encoded_len);
			}
			efree(encoded);
		}
	}
	
	return PSFS_PASS_ON;
}

static PHP_HTTP_FILTER_DESTRUCTOR(stream)
{
	PHP_HTTP_FILTER_BUFFER(stream) *buffer = Z_PTR(this->abstract);
	php_http_encoding_stream_free(&buffer);
}

static PHP_HTTP_FILTER_OPS(chunked_decode) = {
	PHP_HTTP_FILTER_FUNC(chunked_decode),
	PHP_HTTP_FILTER_DTOR(chunked_decode),
	"http.chunked_decode"
};

static PHP_HTTP_FILTER_OPS(chunked_encode) = {
	PHP_HTTP_FILTER_FUNC(chunked_encode),
	NULL,
	"http.chunked_encode"
};

static PHP_HTTP_FILTER_OPS(deflate) = {
	PHP_HTTP_FILTER_FUNC(stream),
	PHP_HTTP_FILTER_DTOR(stream),
	"http.deflate"
};

static PHP_HTTP_FILTER_OPS(inflate) = {
	PHP_HTTP_FILTER_FUNC(stream),
	PHP_HTTP_FILTER_DTOR(stream),
	"http.inflate"
};

#if PHP_HTTP_HAVE_LIBBROTLI
static PHP_HTTP_FILTER_OPS(brotli_encode) = {
	PHP_HTTP_FILTER_FUNC(stream),
	PHP_HTTP_FILTER_DTOR(stream),
	"http.brotli_encode"
};

static PHP_HTTP_FILTER_OPS(brotli_decode) = {
	PHP_HTTP_FILTER_FUNC(stream),
	PHP_HTTP_FILTER_DTOR(stream),
	"http.brotli_decode"
};
#endif

#if PHP_VERSION_ID >= 70200
static php_stream_filter *http_filter_create(const char *name, zval *params, uint8_t p)
#else
static php_stream_filter *http_filter_create(const char *name, zval *params, int p)
#endif
{
	zval *tmp = params;
	php_stream_filter *f = NULL;
	int flags = p ? PHP_HTTP_ENCODING_STREAM_PERSISTENT : 0;
	
	if (params) {
		switch (Z_TYPE_P(params)) {
		case IS_ARRAY:
		case IS_OBJECT:
			if (!(tmp = zend_hash_str_find_ind(HASH_OF(params), ZEND_STRL("flags")))) {
				break;
			}
			/* no break */
		default:
			flags |= zval_get_long(tmp) & 0x0fffffff;
			break;
		}
	}

	if (!strcasecmp(name, "http.chunked_decode")) {
		PHP_HTTP_FILTER_BUFFER(chunked_decode) *b = NULL;
		
		if ((b = pecalloc(1, sizeof(PHP_HTTP_FILTER_BUFFER(chunked_decode)), p))) {
			php_http_buffer_init_ex(PHP_HTTP_BUFFER(b), 4096, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0);
			if (!(f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(chunked_decode), b, p))) {
				pefree(b, p);
			}
		}
	} else
	
	if (!strcasecmp(name, "http.chunked_encode")) {
		f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(chunked_encode), NULL, p);
	} else
	
	if (!strcasecmp(name, "http.inflate")) {
		PHP_HTTP_FILTER_BUFFER(stream) *b = NULL;
		
		if ((b = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_inflate_ops(), flags))) {
			if (!(f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(inflate), b, p))) {
				php_http_encoding_stream_free(&b);
			}
		}
	} else
	
	if (!strcasecmp(name, "http.deflate")) {
		PHP_HTTP_FILTER_BUFFER(stream) *b = NULL;
		
		if ((b = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_deflate_ops(), flags))) {
			if (!(f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(deflate), b, p))) {
				php_http_encoding_stream_free(&b);
			}
		}
#if PHP_HTTP_HAVE_LIBBROTLI
	} else

	if (!strcasecmp(name, "http.brotli_encode")) {
		PHP_HTTP_FILTER_BUFFER(stream) *b = NULL;

		if ((b = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_enbrotli_ops(), flags))) {
			if (!(f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(brotli_encode), b, p))) {
				php_http_encoding_stream_free(&b);
			}
		}
	} else

	if (!strcasecmp(name, "http.brotli_decode")) {
		PHP_HTTP_FILTER_BUFFER(stream) *b = NULL;

		if ((b = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_debrotli_ops(), flags))) {
			if (!(f = php_stream_filter_alloc(&PHP_HTTP_FILTER_OP(brotli_decode), b, p))) {
				php_http_encoding_stream_free(&b);
			}
		}
#endif
	}
	
	return f;
}

php_stream_filter_factory php_http_filter_factory = {
	http_filter_create
};


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
