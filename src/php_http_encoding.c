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

const char *php_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len)
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
				php_error_docref(NULL, E_NOTICE, "Data does not seem to be chunked encoded");
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				php_error_docref(NULL, E_WARNING, "Expected chunk size at pos %tu of %zu but got trash", n_ptr - encoded, encoded_len);
				return NULL;
			}
		}
		
		/* reached the end */
		if (!chunk_len) {
			/* move over '0' chunked encoding terminator and any new lines */
			while(1) {
				switch (*e_ptr) {
					case '0':
					case '\r':
					case '\n':
						++e_ptr;
						continue;
				}
				break;
			}
			break;
		}

		/* there should be CRLF after the chunk size, but we'll ignore SP+ too */
		if (*n_ptr && !eol_match(&n_ptr, &eol_len)) {
			if (eol_len == 2) {
				php_error_docref(NULL, E_WARNING, "Expected CRLF at pos %tu of %zu but got 0x%02X 0x%02X", n_ptr - encoded, encoded_len, *n_ptr, *(n_ptr + 1));
			} else {
				php_error_docref(NULL, E_WARNING, "Expected LF at pos %tu of %zu but got 0x%02X", n_ptr - encoded, encoded_len, *n_ptr);
			}
		}
		n_ptr += eol_len;
		
		/* chunk size pretends more data than we actually got, so it's probably a truncated message */
		if (chunk_len > (rest = encoded + encoded_len - n_ptr)) {
			php_error_docref(NULL, E_WARNING, "Truncated message: chunk size %lu exceeds remaining data size %lu at pos %tu of %zu", chunk_len, rest, n_ptr - encoded, encoded_len);
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

php_http_encoding_stream_t *php_http_encoding_stream_init(php_http_encoding_stream_t *s, php_http_encoding_stream_ops_t *ops, unsigned flags)
{
	int freeme;

	if ((freeme = !s)) {
		s = pemalloc(sizeof(*s), (flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
	}
	memset(s, 0, sizeof(*s));

	s->flags = flags;

	if (EXPECTED(s->ops = ops)) {
		php_http_encoding_stream_t *ss = s->ops->init(s);

		if (EXPECTED(ss)) {
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

php_http_encoding_stream_t *php_http_encoding_stream_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	if (from->ops->copy) {
		php_http_encoding_stream_t *ns;

		if (!to) {
			to = pemalloc(sizeof(*to), (from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT));
		}
		memset(to, 0, sizeof(*to));

		to->flags = from->flags;
		to->ops = from->ops;

		if ((ns = to->ops->copy(from, to))) {
			return ns;
		} else {
			return to;
		}
	}

	return NULL;
}

ZEND_RESULT_CODE php_http_encoding_stream_reset(php_http_encoding_stream_t **s)
{
	php_http_encoding_stream_t *ss;

	if (EXPECTED((*s)->ops->dtor)) {
		(*s)->ops->dtor(*s);
	}

	if (EXPECTED(ss = (*s)->ops->init(*s))) {
		ss->flags &= ~PHP_HTTP_ENCODING_STREAM_DIRTY;
		*s = ss;
		return SUCCESS;
	}
	return FAILURE;
}

ZEND_RESULT_CODE php_http_encoding_stream_update(php_http_encoding_stream_t *s, const char *in_str, size_t in_len, char **out_str, size_t *out_len)
{
	ZEND_RESULT_CODE rc = FAILURE;

	if (EXPECTED(s->ops->update)) {
		rc = s->ops->update(s, in_str, in_len, out_str, out_len);
	}

	s->flags |= PHP_HTTP_ENCODING_STREAM_DIRTY;

	return rc;
}

ZEND_RESULT_CODE php_http_encoding_stream_flush(php_http_encoding_stream_t *s, char **out_str, size_t *out_len)
{
	if (!s->ops->flush) {
		*out_str = NULL;
		*out_len = 0;
		return SUCCESS;
	}
	return s->ops->flush(s, out_str, out_len);
}

zend_bool php_http_encoding_stream_done(php_http_encoding_stream_t *s)
{
	if (!s->ops->done) {
		return !(s->flags & PHP_HTTP_ENCODING_STREAM_DIRTY);
	}
	return s->ops->done(s);
}

ZEND_RESULT_CODE php_http_encoding_stream_finish(php_http_encoding_stream_t *s, char **out_str, size_t *out_len)
{
	if (!s->ops->finish) {
		*out_str = NULL;
		*out_len = 0;

		s->flags &= ~PHP_HTTP_ENCODING_STREAM_DIRTY;

		return SUCCESS;
	}
	return s->ops->finish(s, out_str, out_len);
}

void php_http_encoding_stream_dtor(php_http_encoding_stream_t *s)
{
	if (EXPECTED(s->ops->dtor)) {
		s->ops->dtor(s);
	}
}

void php_http_encoding_stream_free(php_http_encoding_stream_t **s)
{
	if (EXPECTED(*s)) {
		if (EXPECTED((*s)->ops->dtor)) {
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

static php_http_encoding_stream_t *dechunk_copy(php_http_encoding_stream_t *from, php_http_encoding_stream_t *to)
{
	int p = from->flags & PHP_HTTP_ENCODING_STREAM_PERSISTENT;
	struct dechunk_ctx *from_ctx = from->ctx, *to_ctx = pemalloc(sizeof(*to_ctx), p);

	if (php_http_buffer_init_ex(&to_ctx->buffer, PHP_HTTP_BUFFER_DEFAULT_SIZE, p ? PHP_HTTP_BUFFER_INIT_PERSISTENT : 0)) {
		to_ctx->hexlen = from_ctx->hexlen;
		to_ctx->zeroed = from_ctx->zeroed;
		php_http_buffer_append(&to_ctx->buffer, from_ctx->buffer.data, from_ctx->buffer.used);
		to->ctx = to_ctx;
		return to;
	}
	pefree(to_ctx, p);
	php_error_docref(NULL, E_WARNING, "Failed to copy inflate encoding stream: out of memory");
	return NULL;
}

static ZEND_RESULT_CODE dechunk_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len)
{
	php_http_buffer_t tmp;
	struct dechunk_ctx *ctx = s->ctx;

	if (ctx->zeroed) {
		php_error_docref(NULL, E_WARNING, "Dechunk encoding stream has already reached the end of chunked input");
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
						php_error_docref(NULL, E_WARNING, "Failed to parse chunk len from '%.*s'", (int) MIN(16, ctx->buffer.used), ctx->buffer.data);
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

static ZEND_RESULT_CODE dechunk_flush(php_http_encoding_stream_t *s, char **decoded, size_t *decoded_len)
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



static zend_bool dechunk_done(php_http_encoding_stream_t *s)
{
	return ((struct dechunk_ctx *) s->ctx)->zeroed;
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

static php_http_encoding_stream_ops_t php_http_encoding_dechunk_ops = {
	dechunk_init,
	dechunk_copy,
	dechunk_update,
	dechunk_flush,
	dechunk_done,
	NULL,
	dechunk_dtor
};

php_http_encoding_stream_ops_t *php_http_encoding_stream_get_dechunk_ops(void)
{
	return &php_http_encoding_dechunk_ops;
}

static zend_object_handlers php_http_encoding_stream_object_handlers;

zend_object *php_http_encoding_stream_object_new(zend_class_entry *ce)
{
	return &php_http_encoding_stream_object_new_ex(ce, NULL)->zo;
}

php_http_encoding_stream_object_t *php_http_encoding_stream_object_new_ex(zend_class_entry *ce, php_http_encoding_stream_t *s)
{
	php_http_encoding_stream_object_t *o;

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	if (s) {
		o->stream = s;
	}

	o->zo.handlers = &php_http_encoding_stream_object_handlers;

	return o;
}

zend_object *php_http_encoding_stream_object_clone(zval *object)
{
	php_http_encoding_stream_object_t *new_obj, *old_obj = PHP_HTTP_OBJ(NULL, object);
	php_http_encoding_stream_t *cpy = php_http_encoding_stream_copy(old_obj->stream, NULL);

	if (!cpy) {
		return NULL;
	}

	new_obj = php_http_encoding_stream_object_new_ex(old_obj->zo.ce, cpy);
	zend_objects_clone_members(&new_obj->zo, &old_obj->zo);

	return &new_obj->zo;
}

void php_http_encoding_stream_object_free(zend_object *object)
{
	php_http_encoding_stream_object_t *o = PHP_HTTP_OBJ(object, NULL);

	if (o->stream) {
		php_http_encoding_stream_free(&o->stream);
	}
	zend_object_std_dtor(object);
}

static zend_class_entry *php_http_encoding_stream_class_entry;
zend_class_entry *php_http_get_encoding_stream_class_entry(void)
{
	return php_http_encoding_stream_class_entry;
}

static zend_class_entry *php_http_dechunk_stream_class_entry;
zend_class_entry *php_http_get_dechunk_stream_class_entry(void)
{
	return php_http_dechunk_stream_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEncodingStream___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEncodingStream, __construct)
{
	zend_long flags = 0;
	php_http_encoding_stream_object_t *obj;
	php_http_encoding_stream_ops_t *ops;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &flags), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	if (UNEXPECTED(obj->stream)) {
		php_http_throw(bad_method_call, "http\\Encoding\\Stream cannot be initialized twice", NULL);
		return;
	}

	if (instanceof_function(obj->zo.ce, php_http_get_deflate_stream_class_entry())) {
		ops = php_http_encoding_stream_get_deflate_ops();
	} else if (instanceof_function(obj->zo.ce, php_http_get_inflate_stream_class_entry())) {
		ops = php_http_encoding_stream_get_inflate_ops();
	} else if (instanceof_function(obj->zo.ce, php_http_dechunk_stream_class_entry)) {
		ops = &php_http_encoding_dechunk_ops;
#if PHP_HTTP_HAVE_LIBBROTLI
	} else if (instanceof_function(obj->zo.ce, php_http_get_enbrotli_stream_class_entry())) {
		ops = php_http_encoding_stream_get_enbrotli_ops();
	} else if (instanceof_function(obj->zo.ce, php_http_get_debrotli_stream_class_entry())) {
		ops = php_http_encoding_stream_get_debrotli_ops();
#endif
	} else {
		php_http_throw(runtime, "Unknown http\\Encoding\\Stream class '%s'", obj->zo.ce->name->val);
		return;
	}

	php_http_expect(obj->stream = php_http_encoding_stream_init(obj->stream, ops, flags), runtime, return);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEncodingStream_update, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEncodingStream, update)
{
	size_t data_len;
	char *data_str;

	if (EXPECTED(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &data_str, &data_len))) {
		php_http_encoding_stream_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (EXPECTED(obj->stream)) {
			char *encoded_str = NULL;
			size_t encoded_len;

			if (EXPECTED(SUCCESS == php_http_encoding_stream_update(obj->stream, data_str, data_len, &encoded_str, &encoded_len))) {
				if (EXPECTED(encoded_str)) {
					RETURN_STR(php_http_cs2zs(encoded_str, encoded_len));
				} else {
					RETURN_EMPTY_STRING();
				}
			}
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEncodingStream_flush, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEncodingStream, flush)
{
	if (EXPECTED(SUCCESS == zend_parse_parameters_none())) {
		php_http_encoding_stream_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (EXPECTED(obj->stream)) {
			char *encoded_str = NULL;
			size_t encoded_len;

			if (EXPECTED(SUCCESS == php_http_encoding_stream_flush(obj->stream, &encoded_str, &encoded_len))) {
				if (encoded_str) {
					RETURN_STR(php_http_cs2zs(encoded_str, encoded_len));
				} else {
					RETURN_EMPTY_STRING();
				}
			}
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEncodingStream_done, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEncodingStream, done)
{
	if (EXPECTED(SUCCESS == zend_parse_parameters_none())) {
		php_http_encoding_stream_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (EXPECTED(obj->stream)) {
			RETURN_BOOL(php_http_encoding_stream_done(obj->stream));
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEncodingStream_finish, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEncodingStream, finish)
{
	if (EXPECTED(SUCCESS == zend_parse_parameters_none())) {
		php_http_encoding_stream_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (EXPECTED(obj->stream)) {
			char *encoded_str = NULL;
			size_t encoded_len;

			if (EXPECTED(SUCCESS == php_http_encoding_stream_finish(obj->stream, &encoded_str, &encoded_len))) {
				if (EXPECTED(SUCCESS == php_http_encoding_stream_reset(&obj->stream))) {
					if (encoded_str) {
						RETURN_STR(php_http_cs2zs(encoded_str, encoded_len));
					} else {
						RETURN_EMPTY_STRING();
					}
				} else {
					PTR_FREE(encoded_str);
				}
			}
		}
	}
}

static zend_function_entry php_http_encoding_stream_methods[] = {
	PHP_ME(HttpEncodingStream, __construct,  ai_HttpEncodingStream___construct,  ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpEncodingStream, update,       ai_HttpEncodingStream_update,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpEncodingStream, flush,        ai_HttpEncodingStream_flush,        ZEND_ACC_PUBLIC)
	PHP_ME(HttpEncodingStream, done,         ai_HttpEncodingStream_done,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpEncodingStream, finish,       ai_HttpEncodingStream_finish,       ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

ZEND_BEGIN_ARG_INFO_EX(ai_HttpDechunkStream_decode, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(1, decoded_len)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpDechunkStream, decode)
{
	char *str;
	size_t len;
	zval *zlen = NULL;

	if (EXPECTED(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|z!", &str, &len, &zlen))) {
		const char *end_ptr;
		char *enc_str = NULL;
		size_t enc_len;

		if (EXPECTED(end_ptr = php_http_encoding_dechunk(str, len, &enc_str, &enc_len))) {
			if (zlen) {
				ZVAL_DEREF(zlen);
				zval_dtor(zlen);
				ZVAL_LONG(zlen, str + len - end_ptr);
			}
			if (enc_str) {
				RETURN_STR(php_http_cs2zs(enc_str, enc_len));
			} else {
				RETURN_EMPTY_STRING();
			}
		}
	}
	RETURN_FALSE;
}

static zend_function_entry php_http_dechunk_stream_methods[] = {
	PHP_ME(HttpDechunkStream, decode, ai_HttpDechunkStream_decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_encoding)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding", "Stream", php_http_encoding_stream_methods);
	php_http_encoding_stream_class_entry = zend_register_internal_class(&ce);
	php_http_encoding_stream_class_entry->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
	php_http_encoding_stream_class_entry->create_object = php_http_encoding_stream_object_new;
	memcpy(&php_http_encoding_stream_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_encoding_stream_object_handlers.offset = XtOffsetOf(php_http_encoding_stream_object_t, zo);
	php_http_encoding_stream_object_handlers.clone_obj = php_http_encoding_stream_object_clone;
	php_http_encoding_stream_object_handlers.free_obj = php_http_encoding_stream_object_free;

	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_NONE"), PHP_HTTP_ENCODING_STREAM_FLUSH_NONE);
	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_SYNC"), PHP_HTTP_ENCODING_STREAM_FLUSH_SYNC);
	zend_declare_class_constant_long(php_http_encoding_stream_class_entry, ZEND_STRL("FLUSH_FULL"), PHP_HTTP_ENCODING_STREAM_FLUSH_FULL);

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding\\Stream", "Dechunk", php_http_dechunk_stream_methods);
	php_http_dechunk_stream_class_entry = zend_register_internal_class_ex(&ce, php_http_encoding_stream_class_entry);
	php_http_dechunk_stream_class_entry->create_object = php_http_encoding_stream_object_new;

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

