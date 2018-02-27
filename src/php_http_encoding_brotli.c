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

#if PHP_HTTP_HAVE_LIBBROTLI

#define PHP_HTTP_DEBROTLI_ROUNDS 100
#define PHP_HTTP_ENBROTLI_ROUNDS 100

#define PHP_HTTP_ENBROTLI_BUFFER_SIZE_GUESS(S) \
	BrotliEncoderMaxCompressedSize(S)
#define PHP_HTTP_DEBROTLI_BUFFER_SIZE_ALIGN(S) \
	((S) += (S) >> 3)

#define PHP_HTTP_ENBROTLI_FLUSH_FLAG(flags) \
	PHP_HTTP_ENCODING_STREAM_FLUSH_FLAG((flags), BROTLI_OPERATION_FLUSH, BROTLI_OPERATION_FLUSH, BROTLI_OPERATION_PROCESS)

#define PHP_HTTP_ENBROTLI_BUFFER_SIZE		0x2000
#define PHP_HTTP_DEBROTLI_BUFFER_SIZE		0x1000

#define PHP_HTTP_ENBROTLI_LEVEL_SET(flags, level) \
	level = (((flags) & 0xf) ?: PHP_HTTP_ENBROTLI_LEVEL_DEF)
#define PHP_HTTP_ENBROTLI_WBITS_SET(flags, wbits) \
	wbits = ((((flags) >> 4) & 0xff) ?: (PHP_HTTP_ENBROTLI_WBITS_DEF >> 4))
#define PHP_HTTP_ENBROTLI_MODE_SET(flags, mode) \
	mode = (((flags) >> 12) & 0xf)


static php_http_encoding_stream_t *enbrotli_init(php_http_encoding_stream_t *s)
{
	BrotliEncoderState *br;
	int q, win, mode;

	PHP_HTTP_ENBROTLI_LEVEL_SET(s->flags, q);
	PHP_HTTP_ENBROTLI_WBITS_SET(s->flags, win);
	PHP_HTTP_ENBROTLI_MODE_SET(s->flags, mode);

	br = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	if (EXPECTED(br)) {
		BrotliEncoderSetParameter(br, BROTLI_PARAM_QUALITY, q);
		BrotliEncoderSetParameter(br, BROTLI_PARAM_LGWIN, win);
		BrotliEncoderSetParameter(br, BROTLI_PARAM_MODE, mode);

		s->ctx = br;
		return s;
	}

	php_error_docref(NULL, E_WARNING, "Failed to initialize brotli encoding stream");
	return NULL;
}

static ZEND_RESULT_CODE enbrotli_update(php_http_encoding_stream_t *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len)
{
	php_http_buffer_t out;
	const unsigned char *in_ptr;
	size_t in_len;
	BROTLI_BOOL rc;

	php_http_buffer_init_ex(&out, PHP_HTTP_ENBROTLI_BUFFER_SIZE_GUESS(data_len), 0);

	in_len = data_len;
	in_ptr = (const unsigned char *) data;

	while (in_len) {
		size_t out_len = 0;

		rc = BrotliEncoderCompressStream(s->ctx, PHP_HTTP_ENBROTLI_FLUSH_FLAG(s->flags), &in_len, &in_ptr, &out_len, NULL, NULL);

		if (!rc) {
			break;
		}

		if (BrotliEncoderHasMoreOutput(s->ctx)) {
			const char *out_str = (const char *) BrotliEncoderTakeOutput(s->ctx, &out_len);

			php_http_buffer_append(&out, out_str, out_len);
		}
	}

	if (rc) {
		if (out.used) {
			php_http_buffer_shrink(&out);
			php_http_buffer_fix(&out);
			*encoded = out.data;
			*encoded_len = out.used;
		} else {
			*encoded = NULL;
			*encoded_len = 0;
			php_http_buffer_dtor(&out);
		}
		return SUCCESS;
	}

	php_http_buffer_dtor(&out);

	*encoded = NULL;
	*encoded_len = 0;

	php_error_docref(NULL, E_WARNING, "Failed to update brotli encoding stream");
	return FAILURE;
}

static inline ZEND_RESULT_CODE enbrotli_flush_ex(php_http_encoding_stream_t *s, BrotliEncoderOperation op, char **encoded, size_t *encoded_len)
{
	php_http_buffer_t out;
	BROTLI_BOOL rc;
	int round = 0;

	php_http_buffer_init_ex(&out, PHP_HTTP_ENBROTLI_BUFFER_SIZE, 0);

	do {
		const unsigned char *empty = NULL;
		size_t unused = 0, out_len = 0;

		rc = BrotliEncoderCompressStream(s->ctx, op, &unused, &empty, &out_len, NULL, NULL);

		if (!rc) {
			break;
		}

		if (BrotliEncoderHasMoreOutput(s->ctx)) {
			const char *out_str = (const char *) BrotliEncoderTakeOutput(s->ctx, &out_len);

			php_http_buffer_append(&out, out_str, out_len);
		} else {
			if (out.used) {
				php_http_buffer_shrink(&out);
				php_http_buffer_fix(&out);

				*encoded = out.data;
				*encoded_len = out.used;
			} else {
				*encoded = NULL;
				*encoded_len = 0;
			}
			return SUCCESS;
		}
	} while (++round < PHP_HTTP_ENBROTLI_ROUNDS);

	php_http_buffer_dtor(&out);

	*encoded = NULL;
	*encoded_len = 0;

	php_error_docref(NULL, E_WARNING, "Failed to flush brotli encoding stream");
	return FAILURE;
}

static ZEND_RESULT_CODE enbrotli_flush(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	return enbrotli_flush_ex(s, BROTLI_OPERATION_FLUSH, encoded, encoded_len);
}

static ZEND_RESULT_CODE enbrotli_finish(php_http_encoding_stream_t *s, char **encoded, size_t *encoded_len)
{
	ZEND_RESULT_CODE rc;

	do {
		rc = enbrotli_flush_ex(s, BROTLI_OPERATION_FINISH, encoded, encoded_len);
	} while (SUCCESS == rc && !BrotliEncoderIsFinished(s->ctx));

	return rc;
}

static zend_bool enbrotli_done(php_http_encoding_stream_t *s)
{
	return !(s->flags & PHP_HTTP_ENCODING_STREAM_DIRTY)
			|| BrotliEncoderIsFinished(s->ctx);
}

static void enbrotli_dtor(php_http_encoding_stream_t *s)
{
	if (s->ctx) {
		BrotliEncoderDestroyInstance(s->ctx);
		s->ctx = NULL;
	}
}

static php_http_encoding_stream_t *debrotli_init(php_http_encoding_stream_t *s)
{
	BrotliDecoderState *br;

	br = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	if (br) {
		s->ctx = br;
		return s;
	}

	php_error_docref(NULL, E_WARNING, "Failed to initialize brotli decoding stream");
	return NULL;
}

static ZEND_RESULT_CODE debrotli_update(php_http_encoding_stream_t *s, const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len)
{
	php_http_buffer_t out;
	BrotliDecoderResult rc;
	const unsigned char *in_ptr;
	size_t in_len;

	in_ptr = (const unsigned char *) encoded;
	in_len = encoded_len;

	php_http_buffer_init_ex(&out, encoded_len, PHP_HTTP_BUFFER_INIT_PREALLOC);

	while (in_len) {
		size_t out_len = 0;

		rc = BrotliDecoderDecompressStream(s->ctx, &in_len, &in_ptr, &out_len, NULL, NULL);

		if (BROTLI_DECODER_RESULT_ERROR == rc) {
			break;
		}

		if (BrotliDecoderHasMoreOutput(s->ctx)) {
			const char *out_str = (const char *) BrotliDecoderTakeOutput(s->ctx, &out_len);

			php_http_buffer_append(&out, out_str, out_len);
		}
	}

	if (BROTLI_DECODER_RESULT_ERROR != rc) {
		if (out.used) {
			php_http_buffer_shrink(&out);
			php_http_buffer_fix(&out);
			*decoded = out.data;
			*decoded_len = out.used;
		} else {
			php_http_buffer_dtor(&out);
			*decoded = NULL;
			*decoded_len = 0;
		}
		return SUCCESS;
	}

	php_http_buffer_dtor(&out);

	php_error_docref(NULL, E_WARNING, "Could not brotli decode data: %s", BrotliDecoderErrorString(BrotliDecoderGetErrorCode(s->ctx)));
	return FAILURE;
}

static zend_bool debrotli_done(php_http_encoding_stream_t *s)
{
	return !BrotliDecoderIsUsed(s->ctx) || BrotliDecoderIsFinished(s->ctx);
}

static void debrotli_dtor(php_http_encoding_stream_t *s)
{
	if (s->ctx) {
		BrotliDecoderDestroyInstance(s->ctx);
		s->ctx = NULL;
	}
}

static php_http_encoding_stream_ops_t php_http_encoding_enbrotli_ops = {
	enbrotli_init,
	NULL,
	enbrotli_update,
	enbrotli_flush,
	enbrotli_done,
	enbrotli_finish,
	enbrotli_dtor
};

php_http_encoding_stream_ops_t *php_http_encoding_stream_get_enbrotli_ops(void)
{
	return &php_http_encoding_enbrotli_ops;
}

static php_http_encoding_stream_ops_t php_http_encoding_debrotli_ops = {
	debrotli_init,
	NULL,
	debrotli_update,
	NULL,
	debrotli_done,
	NULL,
	debrotli_dtor
};

php_http_encoding_stream_ops_t *php_http_encoding_stream_get_debrotli_ops(void)
{
	return &php_http_encoding_debrotli_ops;
}

ZEND_RESULT_CODE php_http_encoding_enbrotli(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len)
{
	BROTLI_BOOL rc;
	int q, win, mode;

	*encoded_len = PHP_HTTP_ENBROTLI_BUFFER_SIZE_GUESS(data_len);
	*encoded = emalloc(*encoded_len + 1);

	PHP_HTTP_ENBROTLI_LEVEL_SET(flags, q);
	PHP_HTTP_ENBROTLI_WBITS_SET(flags, win);
	PHP_HTTP_ENBROTLI_MODE_SET(flags, mode);

	rc = BrotliEncoderCompress(q, win, mode, data_len, (const unsigned char *) data, encoded_len, (unsigned char *) *encoded);
	if (rc) {
		return SUCCESS;
	}

	PTR_SET(*encoded, NULL);
	*encoded_len = 0;

	php_error_docref(NULL, E_WARNING, "Could not brotli encode data");
	return FAILURE;
}

ZEND_RESULT_CODE php_http_encoding_debrotli(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len)
{
	php_http_encoding_stream_t s = {0};
	ZEND_RESULT_CODE rc = FAILURE;

	if (debrotli_init(&s)) {
		rc = debrotli_update(&s, encoded, encoded_len, decoded, decoded_len);
		debrotli_dtor(&s);
	}

	return rc;
}

static zend_class_entry *php_http_enbrotli_stream_class_entry;
zend_class_entry *php_http_get_enbrotli_stream_class_entry(void)
{
	return php_http_enbrotli_stream_class_entry;
}

static zend_class_entry *php_http_debrotli_stream_class_entry;
zend_class_entry *php_http_get_debrotli_stream_class_entry(void)
{
	return php_http_debrotli_stream_class_entry;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnbrotliStream_encode, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnbrotliStream, encode)
{
	char *str;
	size_t len;
	zend_long flags = PHP_HTTP_ENBROTLI_MODE_GENERIC | PHP_HTTP_ENBROTLI_WBITS_DEF | PHP_HTTP_ENBROTLI_LEVEL_DEF;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &str, &len, &flags)) {
		char *enc_str = NULL;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_enbrotli(flags, str, len, &enc_str, &enc_len)) {
			if (enc_str) {
				RETURN_STR(php_http_cs2zs(enc_str, enc_len));
			} else {
				RETURN_EMPTY_STRING();
			}
		}
	}
	RETURN_FALSE;
}
static zend_function_entry php_http_enbrotli_stream_methods[] = {
	PHP_ME(HttpEnbrotliStream, encode, ai_HttpEnbrotliStream_encode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

ZEND_BEGIN_ARG_INFO_EX(ai_HttpDebrotliStream_decode, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpDebrotliStream, decode)
{
	char *str;
	size_t len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &len)) {
		char *enc_str = NULL;
		size_t enc_len;

		if (SUCCESS == php_http_encoding_debrotli(str, len, &enc_str, &enc_len)) {
			if (enc_str) {
				RETURN_STR(php_http_cs2zs(enc_str, enc_len));
			} else {
				RETURN_EMPTY_STRING();
			}
		}
	}
	RETURN_FALSE;
}

static zend_function_entry php_http_debrotli_stream_methods[] = {
	PHP_ME(HttpDebrotliStream, decode, ai_HttpDebrotliStream_decode, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	EMPTY_FUNCTION_ENTRY
};

/* POS */
const void *BrotliGetDictionary();
const void *(*php_http_brotli_get_dictionary)();

PHP_MINIT_FUNCTION(http_encoding_brotli)
{
	zend_class_entry ce;

	/* force link to libbrotlicommon, because their libraries don't */
	php_http_brotli_get_dictionary = BrotliGetDictionary();

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding\\Stream", "Enbrotli", php_http_enbrotli_stream_methods);
	php_http_enbrotli_stream_class_entry = zend_register_internal_class_ex(&ce, php_http_get_encoding_stream_class_entry());
	php_http_enbrotli_stream_class_entry->create_object = php_http_encoding_stream_object_new;

	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("LEVEL_MIN"), PHP_HTTP_ENBROTLI_LEVEL_MIN);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("LEVEL_DEF"), PHP_HTTP_ENBROTLI_LEVEL_DEF);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("LEVEL_MAX"), PHP_HTTP_ENBROTLI_LEVEL_MAX);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("WBITS_MIN"), PHP_HTTP_ENBROTLI_WBITS_MIN);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("WBITS_DEF"), PHP_HTTP_ENBROTLI_WBITS_DEF);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("WBITS_MAX"), PHP_HTTP_ENBROTLI_WBITS_MAX);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("MODE_GENERIC"), PHP_HTTP_ENBROTLI_MODE_GENERIC);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("MODE_TEXT"), PHP_HTTP_ENBROTLI_MODE_TEXT);
	zend_declare_class_constant_long(php_http_enbrotli_stream_class_entry, ZEND_STRL("MODE_FONT"), PHP_HTTP_ENBROTLI_MODE_FONT);

	memset(&ce, 0, sizeof(ce));
	INIT_NS_CLASS_ENTRY(ce, "http\\Encoding\\Stream", "Debrotli", php_http_debrotli_stream_methods);
	php_http_debrotli_stream_class_entry = zend_register_internal_class_ex(&ce, php_http_get_encoding_stream_class_entry());
	php_http_debrotli_stream_class_entry->create_object = php_http_encoding_stream_object_new;

	return SUCCESS;
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

