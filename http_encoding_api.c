/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php_http.h"

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_send_api.h"
#include "php_http_headers_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

static inline int eol_match(char **line, int *eol_len)
{
	char *ptr = *line;
	
	while (0x20 == *ptr) ++ptr;

	if (ptr == http_locate_eol(*line, eol_len)) {
		*line = ptr;
		return 1;
	} else {
		return 0;
	}
}
			
/* {{{ char *http_encoding_dechunk(char *, size_t, char **, size_t *) */
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int eol_len = 0;
	char *n_ptr = NULL;
	const char *e_ptr = encoded;
	
	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len);

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
				http_error(HE_NOTICE, HTTP_E_ENCODING, "Data does not seem to be chunked encoded");
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected chunk size at pos %tu of %zu but got trash", n_ptr - encoded, encoded_len);
				return NULL;
			}
		}
		
		/* reached the end */
		if (!chunk_len) {
			/* move over '0' chunked encoding terminator */
			while (*e_ptr == '0') ++e_ptr;
			break;
		}

		/* there should be CRLF after the chunk size, but we'll ignore SP+ too */
		if (*n_ptr && !eol_match(&n_ptr, &eol_len)) {
			if (eol_len == 2) {
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected CRLF at pos %tu of %zu but got 0x%02X 0x%02X", n_ptr - encoded, encoded_len, *n_ptr, *(n_ptr + 1));
			} else {
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected LF at pos %tu of %zu but got 0x%02X", n_ptr - encoded, encoded_len, *n_ptr);
			}
		}
		n_ptr += eol_len;
		
		/* chunk size pretends more data than we actually got, so it's probably a truncated message */
		if (chunk_len > (rest = encoded + encoded_len - n_ptr)) {
			http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Truncated message: chunk size %lu exceeds remaining data size %lu at pos %tu of %zu", chunk_len, rest, n_ptr - encoded, encoded_len);
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
/* }}} */

#ifdef HTTP_HAVE_ZLIB

static const char http_encoding_gzip_header[] = {
	(const char) 0x1f,			// fixed value
	(const char) 0x8b,			// fixed value
	(const char) Z_DEFLATED,	// compression algorithm
	(const char) 0,				// none of the possible flags defined by the GZIP "RFC"
	(const char) 0,				// MTIME
	(const char) 0,				// =*=
	(const char) 0,				// =*=
	(const char) 0,				// =*=
	(const char) 0,				// two possible flag values for 9 compression levels? o_O
#ifdef PHP_WIN32
	(const char) 0x0b			// OS_CODE
#else
	(const char) 0x03			// OS_CODE
#endif
};

PHP_HTTP_API STATUS _http_encoding_gzencode(int level, int mtime, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	z_stream Z;
	STATUS status = Z_OK;
	
	if (!(data && data_len)) {
		return FAILURE;
	}
	
	Z.zalloc = Z_NULL;
	Z.zfree  = Z_NULL;
	Z.opaque = Z_NULL;
	Z.next_in   = (Bytef *) data;
	Z.avail_in  = data_len;
	Z.avail_out = HTTP_ENCODING_BUFLEN(data_len) + HTTP_ENCODING_SAFPAD - 1;
	
	*encoded = emalloc(HTTP_ENCODING_BUFLEN(data_len) + sizeof(http_encoding_gzip_header) + HTTP_ENCODING_SAFPAD);
	memcpy(*encoded, http_encoding_gzip_header, sizeof(http_encoding_gzip_header));
	
	if (mtime) {
		(*encoded)[4] = (char) (mtime & 0xFF);
		(*encoded)[5] = (char) ((mtime >> 8) & 0xFF);
		(*encoded)[6] = (char) ((mtime >> 16) & 0xFF);
		(*encoded)[7] = (char) ((mtime >> 24) & 0xFF);
	}
	
	Z.next_out = (Bytef *) *encoded + sizeof(http_encoding_gzip_header);
	
	if (Z_OK == (status = deflateInit2(&Z, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))) {
		status = deflate(&Z, Z_FINISH);
		deflateEnd(&Z);
		
		if (Z_STREAM_END == status) {
			ulong crc;
			char *trailer;
			
			crc = crc32(0L, Z_NULL, 0);
			crc = crc32(crc, (const Bytef *) data, data_len);
			
			trailer = *encoded + sizeof(http_encoding_gzip_header) + Z.total_out;
			
			/* LSB */
			trailer[0] = (char) (crc & 0xFF);
			trailer[1] = (char) ((crc >> 8) & 0xFF);
			trailer[2] = (char) ((crc >> 16) & 0xFF);
			trailer[3] = (char) ((crc >> 24) & 0xFF);
			trailer[4] = (char) ((Z.total_in) & 0xFF);
			trailer[5] = (char) ((Z.total_in >> 8) & 0xFF);
			trailer[6] = (char) ((Z.total_in >> 16) & 0xFF);
			trailer[7] = (char) ((Z.total_in >> 24) & 0xFF);
			
			*encoded_len = Z.total_out + sizeof(http_encoding_gzip_header) + 8;
			(*encoded)[*encoded_len] = '\0';
			return SUCCESS;
		}
	}
	
	efree(*encoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not gzencode data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_gzdecode(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *encoded;
	size_t encoded_len;
	
	if (	(data && data_len) &&
			(SUCCESS == http_encoding_gzencode_verify(data, data_len, &encoded, &encoded_len)) &&
			(SUCCESS == http_encoding_inflate(encoded, encoded_len, decoded, decoded_len))) {
		http_encoding_gzdecode_verify(data, data_len, *decoded, *decoded_len);
		return SUCCESS;
	}
	
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_deflate(int level, int zhdr, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	z_stream Z;
	STATUS status = Z_OK;
	
	Z.zalloc = Z_NULL;
	Z.zfree  = Z_NULL;
	Z.opaque = Z_NULL;
	Z.data_type = Z_UNKNOWN;
	Z.next_in   = (Bytef *) data;
	Z.avail_in  = data_len;
	Z.avail_out = HTTP_ENCODING_BUFLEN(data_len) - 1;
	Z.next_out  = emalloc(HTTP_ENCODING_BUFLEN(data_len));
	
	*encoded = (char *) Z.next_out;
	
	if (Z_OK == (status = deflateInit2(&Z, level, Z_DEFLATED, zhdr ? MAX_WBITS : -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))) {
		status = deflate(&Z, Z_FINISH);
		deflateEnd(&Z);
		
		if (Z_STREAM_END == status) {
			(*encoded)[*encoded_len = Z.total_out] = '\0';
			return SUCCESS;
		}
	}
	
	efree(encoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not deflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int max = 0, wbits = -MAX_WBITS;
	STATUS status;
	z_stream Z;
	
	do {
		Z.zalloc = Z_NULL;
		Z.zfree  = Z_NULL;
		
		if (!max) {
			*decoded_len = data_len * 2;
			*decoded = emalloc(*decoded_len + 1);
		} else {
			size_t new_len = *decoded_len << 2;
			char *new_ptr = erealloc_recoverable(*decoded, new_len + 1);
			
			if (new_ptr) {
				*decoded = new_ptr;
				*decoded_len = new_len;
			} else {
				max = INT_MAX-1; /* avoid integer overflow on increment op */
			}
		}
		
retry_inflate:
		Z.next_in   = (Bytef *) data;
		Z.avail_in  = data_len;
		Z.next_out  = (Bytef *) *decoded;
		Z.avail_out = *decoded_len;
		
		if (Z_OK == (status = inflateInit2(&Z, wbits))) {
			status = inflate(&Z, Z_FINISH);
			inflateEnd(&Z);
			
			/* retry if it looks like we've got a zlib header */
			if (wbits == -MAX_WBITS && status == Z_DATA_ERROR) {
				wbits = MAX_WBITS;
				goto retry_inflate;
			}
			
			if (Z_STREAM_END == status) {
				(*decoded)[*decoded_len = Z.total_out] = '\0';
				return SUCCESS;
			}
		}
	} while (status == Z_BUF_ERROR && ++max < HTTP_ENCODING_MAXTRY);
	
	efree(*decoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not inflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_gzencode_verify(const char *data, size_t data_len, const char **encoded, size_t *encoded_len, int error_level TSRMLS_DC)
{
	size_t offset = sizeof(http_encoding_gzip_header);
	
	if (data_len < offset) {
		goto really_bad_gzip_header;
	}
	
	if (data[0] != (const char) 0x1F || data[1] != (const char) 0x8B) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Unrecognized GZIP header start: 0x%02X 0x%02X", (int) data[0], (int) (data[1] & 0xFF));
		return FAILURE;
	}
	
	if (data[2] != (const char) Z_DEFLATED) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Unrecognized compression format (%d)", (int) (data[2] & 0xFF));
		/* still try to decode */
	}
	if ((data[3] & 0x4) == 0x4) {
		if (data_len < offset + 2) {
			goto really_bad_gzip_header;
		}
		/* there are extra fields, the length follows the common header as 2 bytes LSB */
		offset += (unsigned) ((data[offset] & 0xFF));
		offset += 1;
		offset += (unsigned) ((data[offset] & 0xFF) << 8);
		offset += 1;
	}
	if ((data[3] & 0x8) == 0x8) {
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		}
		/* there's a file name */
		offset += strlen(&data[offset]) + 1 /*NUL*/;
	}
	if ((data[3] & 0x10) == 0x10) {
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		}
		/* there's a comment */
		offset += strlen(&data[offset]) + 1 /* NUL */;
	}
	if ((data[3] & 0x2) == 0x2) {
		/* there's a CRC16 of the header */
		offset += 2;
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		} else {
			ulong crc, cmp;
			
			cmp =  (unsigned) ((data[offset-2] & 0xFF));
			cmp += (unsigned) ((data[offset-1] & 0xFF) << 8);
			
			crc = crc32(0L, Z_NULL, 0);
			crc = crc32(crc, (const Bytef *) data, sizeof(http_encoding_gzip_header));
			
			if (cmp != (crc & 0xFFFF)) {
				http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "GZIP headers CRC checksums so not match (%lu, %lu)", cmp, crc & 0xFFFF);
				return FAILURE;
			}
		}
	}
	
	if (data_len < offset + 8) {
		http_error(error_level TSRMLS_CC, HTTP_E_ENCODING, "Missing or truncated GZIP footer");
		return FAILURE;
	}
	
	if (encoded) {
		*encoded = data + offset;
	}
	if (encoded_len) {
		*encoded_len = data_len - offset - 8 /* size of the assumed GZIP footer */;	
	}
	
	return SUCCESS;
	
really_bad_gzip_header:
	http_error(error_level TSRMLS_CC, HTTP_E_ENCODING, "Missing or truncated GZIP header");
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_gzdecode_verify(const char *data, size_t data_len, const char *decoded, size_t decoded_len, int error_level TSRMLS_DC)
{
	STATUS status = SUCCESS;
	ulong len, cmp, crc;
	
	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (const Bytef *) decoded, decoded_len);
	
	cmp  = (unsigned) ((data[data_len-8] & 0xFF));
	cmp += (unsigned) ((data[data_len-7] & 0xFF) << 8);
	cmp += (unsigned) ((data[data_len-6] & 0xFF) << 16);
	cmp += (unsigned) ((data[data_len-5] & 0xFF) << 24);
	len  = (unsigned) ((data[data_len-4] & 0xFF));
	len += (unsigned) ((data[data_len-3] & 0xFF) << 8);
	len += (unsigned) ((data[data_len-2] & 0xFF) << 16);
	len += (unsigned) ((data[data_len-1] & 0xFF) << 24);
	
	if (cmp != crc) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Could not verify data integrity: CRC checksums do not match (%lu, %lu)", cmp, crc);
		status = FAILURE;
	}
	if (len != decoded_len) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Could not verify data integrity: data sizes do not match (%lu, %lu)", len, decoded_len);
		status = FAILURE;
	}
	return status;
}

#define HTTP_ENCODING_STREAM_ERROR(status, tofree) \
	{ \
		if (tofree) efree(tofree); \
		http_error_ex(HE_WARNING, HTTP_E_ENCODING, "GZIP stream error: %s", zError(status)); \
		return FAILURE; \
	}

PHP_HTTP_API STATUS _http_encoding_stream_init(http_encoding_stream *s, int gzip, int level, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	STATUS status;
	
	memset(s, 0, sizeof(http_encoding_stream));
	if (Z_OK != (status = deflateInit2(&s->Z, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))) {
		HTTP_ENCODING_STREAM_ERROR(status, NULL);
	}
	
	if ((s->gzip = gzip)) {
		s->crc = crc32(0L, Z_NULL, 0);
		*encoded_len = sizeof(http_encoding_gzip_header);
		*encoded = emalloc(*encoded_len);
		memcpy(*encoded, http_encoding_gzip_header, *encoded_len);
	} else {
		*encoded_len = 0;
		*encoded = NULL;
	}
	
	return SUCCESS;
}

PHP_HTTP_API STATUS _http_encoding_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	STATUS status;
	
	*encoded_len = HTTP_ENCODING_BUFLEN(data_len);
	*encoded = emalloc(*encoded_len);
	
	s->Z.next_in = (Bytef *) data;
	s->Z.avail_in = data_len;
	s->Z.next_out = (Bytef *) *encoded;
	s->Z.avail_out = *encoded_len;
	
	status = deflate(&s->Z, Z_SYNC_FLUSH);
	
	if (Z_OK != status && Z_STREAM_END != status) {
		HTTP_ENCODING_STREAM_ERROR(status, *encoded);
	}
	*encoded_len -= s->Z.avail_out;
	
	if (s->gzip) {
		s->crc = crc32(s->crc, (const Bytef *) data, data_len);
	}
	
	return SUCCESS;
}

PHP_HTTP_API STATUS _http_encoding_stream_finish(http_encoding_stream *s, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	STATUS status;
	
	*encoded_len = 1024;
	*encoded = emalloc(*encoded_len);
	
	s->Z.next_out = (Bytef *) *encoded;
	s->Z.avail_out = *encoded_len;
	
	if (Z_STREAM_END != (status = deflate(&s->Z, Z_FINISH)) || Z_OK != (status = deflateEnd(&s->Z))) {
		HTTP_ENCODING_STREAM_ERROR(status, *encoded);
	}
	
	*encoded_len -= s->Z.avail_out;
	if (s->gzip) {
		if (s->Z.avail_out < 8) {
			*encoded = erealloc(*encoded, *encoded_len + 8);
		}
		(*encoded)[(*encoded_len)++] = (char) (s->crc & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->crc >> 8) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->crc >> 16) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->crc >> 24) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->Z.total_in) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->Z.total_in >> 8) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->Z.total_in >> 16) & 0xFF);
		(*encoded)[(*encoded_len)++] = (char) ((s->Z.total_in >> 24) & 0xFF);
	}
	
	return SUCCESS;
}

#endif /* HTTP_HAVE_ZLIB */

PHP_HTTP_API zend_bool _http_encoding_response_start(size_t content_length TSRMLS_DC)
{
	if (	php_ob_handler_used("ob_gzhandler" TSRMLS_CC) ||
			php_ob_handler_used("zlib output compression" TSRMLS_CC)) {
		HTTP_G(send).gzip_encoding = 0;
	} else {
		if (!HTTP_G(send).gzip_encoding) {
			/* emit a content-length header */
			if (content_length) {
				char cl_header_str[128];
				size_t cl_header_len;
				cl_header_len = snprintf(cl_header_str, lenof(cl_header_str), "Content-Length: %zu", content_length);
				http_send_header_string_ex(cl_header_str, cl_header_len, 1);
			}
		} else {
#ifndef HTTP_HAVE_ZLIB
			HTTP_G(send).gzip_encoding = 0;
			php_start_ob_buffer_named("ob_gzhandler", 0, 0 TSRMLS_CC);
#else
			HashTable *selected;
			zval zsupported;
			
			INIT_PZVAL(&zsupported);
			array_init(&zsupported);
			add_next_index_stringl(&zsupported, "gzip", lenof("gzip"), 1);
			add_next_index_stringl(&zsupported, "deflate", lenof("deflate"), 1);
			
			HTTP_G(send).gzip_encoding = 0;
			
			if ((selected = http_negotiate_encoding(&zsupported))) {
				STATUS hs = FAILURE;
				char *encoding = NULL;
				ulong idx;
				
				if (HASH_KEY_IS_STRING == zend_hash_get_current_key(selected, &encoding, &idx, 0) && encoding) {
					if (!strcmp(encoding, "gzip")) {
						if (SUCCESS == (hs = http_send_header_string("Content-Encoding: gzip"))) {
							HTTP_G(send).gzip_encoding = HTTP_ENCODING_GZIP;
						}
					} else if (!strcmp(encoding, "deflate")) {
						if (SUCCESS == (hs = http_send_header_string("Content-Encoding: deflate"))) {
							HTTP_G(send).gzip_encoding = HTTP_ENCODING_DEFLATE;
						}
					}
					if (SUCCESS == hs) {
						http_send_header_string("Vary: Accept-Encoding");
					}
				}
				
				zend_hash_destroy(selected);
				FREE_HASHTABLE(selected);
			}
			
			zval_dtor(&zsupported);
			return HTTP_G(send).gzip_encoding;
#endif
		}
	}
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

