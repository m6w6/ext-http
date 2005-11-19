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

#ifndef PHP_HTTP_CACHE_API_H
#define PHP_HTTP_CACHE_API_H

#include "zend_ini.h"

#include "ext/standard/md5.h"
#include "ext/standard/sha1.h"
#include "ext/standard/crc32.h"

#include "php_http_std_defs.h"
#include "php_http.h"
#include "php_http_api.h"
#include "php_http_send_api.h"

#if defined(HAVE_HASH_EXT) && !defined(COMPILE_DL_HASH) && defined(HTTP_HAVE_HASH_EXT_INCLUDES)
#	define HTTP_HAVE_HASH_EXT
#	include "php_hash.h"
#	include "php_hash_sha.h"
#	include "php_hash_ripemd.h"
#endif

#ifdef HTTP_HAVE_MHASH
#	include <mhash.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

typedef enum {
#ifdef HTTP_HAVE_HASH_EXT
	HTTP_ETAG_RIPEMD160 = -8,
	HTTP_ETAG_RIPEMD128 = -7,
	HTTP_ETAG_SHA512 = -6,
	HTTP_ETAG_SHA384 = -5,
	HTTP_ETAG_SHA256 = -4,
#endif
	HTTP_ETAG_CRC32 = -3,
	HTTP_ETAG_MD5 = -2,
	HTTP_ETAG_SHA1 = -1,
} http_etag_mode;

extern PHP_MINIT_FUNCTION(http_cache);

#ifdef HTTP_HAVE_MHASH
static void *http_etag_alloc_mhash_digest(size_t size)
{
	return emalloc(size);
}
#endif

#define http_etag_digest(d, l) _http_etag_digest((d), (l) TSRMLS_CC)
static inline char *_http_etag_digest(const unsigned char *digest, int len TSRMLS_DC)
{
	static const char hexdigits[16] = "0123456789abcdef";
	int i;
	char *hex = emalloc(len * 2 + 1);
	char *ptr = hex;
	
	for (i = 0; i < len; ++i) {
		*ptr++ = hexdigits[digest[i] >> 4];
		*ptr++ = hexdigits[digest[i] & 0xF];
	}
	*ptr = '\0';
	
	return hex;
}

#undef CASE_HTTP_ETAG_HASH
#define CASE_HTTP_ETAG_HASH(HASH) \
	case HTTP_ETAG_##HASH: \
		PHP_##HASH##Init(ctx = emalloc(sizeof(PHP_##HASH##_CTX))); \
	break;
#define http_etag_init() _http_etag_init(TSRMLS_C)
static inline void *_http_etag_init(TSRMLS_D)
{
	void *ctx = NULL;
	long mode = HTTP_G(etag).mode;

	switch (mode)
	{
		case HTTP_ETAG_CRC32:
			ctx = emalloc(sizeof(uint));
			*((uint *) ctx) = ~0;
		break;
		
#ifdef HTTP_HAVE_HASH_EXT
		CASE_HTTP_ETAG_HASH(RIPEMD160);
		CASE_HTTP_ETAG_HASH(RIPEMD128);
		CASE_HTTP_ETAG_HASH(SHA512);
		CASE_HTTP_ETAG_HASH(SHA384);
		CASE_HTTP_ETAG_HASH(SHA256);
#endif
		CASE_HTTP_ETAG_HASH(SHA1);
#ifndef HTTP_HAVE_MHASH
		default:
#endif
		CASE_HTTP_ETAG_HASH(MD5);
		
#ifdef HTTP_HAVE_MHASH
		default:
			if ((mode < 0) || ((ulong)mode > mhash_count()) || (!(ctx = mhash_init(mode)))) {
				http_error_ex(HE_ERROR, HTTP_E_RUNTIME, "Invalid ETag mode: %ld", mode);
			}
		break;
#endif
	}
	
	return ctx;
}

#undef CASE_HTTP_ETAG_HASH
#define CASE_HTTP_ETAG_HASH(HASH) \
	case HTTP_ETAG_##HASH: \
		if (*((PHP_##HASH##_CTX **) ctx_ptr)) { \
			efree(*((PHP_##HASH##_CTX **) ctx_ptr)); \
			*((PHP_##HASH##_CTX **) ctx_ptr) = NULL; \
		} \
	break;
#define http_etag_free(cp) _http_etag_free((cp) TSRMLS_CC)
static inline void _http_etag_free(void **ctx_ptr TSRMLS_DC)
{
	switch (HTTP_G(etag).mode)
	{
		case HTTP_ETAG_CRC32:
			if (*((uint **) ctx_ptr)) {
				efree(*((uint **) ctx_ptr));
				*((uint **) ctx_ptr) = NULL;
			}
		break;

#ifdef HTTP_HAVE_HASH_EXT
		CASE_HTTP_ETAG_HASH(RIPEMD160);
		CASE_HTTP_ETAG_HASH(RIPEMD128);
		CASE_HTTP_ETAG_HASH(SHA512);
		CASE_HTTP_ETAG_HASH(SHA384);
		CASE_HTTP_ETAG_HASH(SHA256);
#endif
		CASE_HTTP_ETAG_HASH(SHA1);
#ifndef HTTP_HAVE_MHASH
		default:
#endif
		CASE_HTTP_ETAG_HASH(MD5);
		
#ifdef HTTP_HAVE_MHASH
		default:
			/* mhash gets already freed in http_etag_finish() */
			if (*((MHASH *) ctx_ptr)) {
				mhash_deinit(*((MHASH *) ctx_ptr), NULL);
				*((MHASH *) ctx_ptr) = NULL;
			}
		break;
#endif
	}
}

#undef CASE_HTTP_ETAG_HASH
#define CASE_HTTP_ETAG_HASH(HASH, len) \
	case HTTP_ETAG_##HASH##: \
		PHP_##HASH##Final(digest, *((PHP_##HASH##_CTX **) ctx_ptr)); \
		etag = http_etag_digest(digest, len); \
	break;
#define http_etag_finish(c) _http_etag_finish((c) TSRMLS_CC)
static inline char *_http_etag_finish(void **ctx_ptr TSRMLS_DC)
{
	char *etag = NULL;
	unsigned char digest[128];
	long mode = HTTP_G(etag).mode;
	
	switch (mode)
	{
		case HTTP_ETAG_CRC32:
			**((uint **) ctx_ptr) = ~**((uint **) ctx_ptr);
			etag = http_etag_digest(*((const unsigned char **) ctx_ptr), sizeof(uint));
		break;

#ifdef HTTP_HAVE_HASH_EXT
		CASE_HTTP_ETAG_HASH(RIPEMD160, 20);
		CASE_HTTP_ETAG_HASH(RIPEMD128, 16);
		CASE_HTTP_ETAG_HASH(SHA512, 64);
		CASE_HTTP_ETAG_HASH(SHA384, 48);
		CASE_HTTP_ETAG_HASH(SHA256, 32);
#endif
		CASE_HTTP_ETAG_HASH(SHA1, 20);
#ifndef HTTP_HAVE_MHASH
		default:
#endif
		CASE_HTTP_ETAG_HASH(MD5, 16);
		
#ifdef HTTP_HAVE_MHASH
		default:
		{
			unsigned char *mhash_digest = mhash_end_m(*((MHASH *) ctx_ptr), http_etag_alloc_mhash_digest);
			etag = http_etag_digest(mhash_digest, mhash_get_block_size(mode));
			efree(mhash_digest);
			/* avoid double free */
			*((MHASH *) ctx_ptr) = NULL;
		}
		break;
#endif
	}
	
	http_etag_free(ctx_ptr);
	
	return etag;
}

#undef CASE_HTTP_ETAG_HASH
#define CASE_HTTP_ETAG_HASH(HASH) \
	case HTTP_ETAG_##HASH: \
		PHP_##HASH##Update(ctx, (const unsigned char *) data_ptr, data_len); \
	break;
#define http_etag_update(c, d, l) _http_etag_update((c), (d), (l) TSRMLS_CC)
static inline void _http_etag_update(void *ctx, const char *data_ptr, size_t data_len TSRMLS_DC)
{
	switch (HTTP_G(etag).mode)
	{
		case HTTP_ETAG_CRC32:
		{
			uint i, c = *((uint *) ctx);
			
			for (i = 0; i < data_len; ++i) {
				c = CRC32(c, data_ptr[i]);
			}
			*((uint *)ctx) = c;
		}
		break;
		
#ifdef HTTP_HAVE_HASH_EXT
		CASE_HTTP_ETAG_HASH(RIPEMD160);
		CASE_HTTP_ETAG_HASH(RIPEMD128);
		CASE_HTTP_ETAG_HASH(SHA512);
		CASE_HTTP_ETAG_HASH(SHA384);
		CASE_HTTP_ETAG_HASH(SHA256);
#endif
		CASE_HTTP_ETAG_HASH(SHA1);
#ifndef HTTP_HAVE_MHASH
		default:
#endif
		CASE_HTTP_ETAG_HASH(MD5);
		
#ifdef HTTP_HAVE_MHASH
		default:
			mhash(ctx, data_ptr, data_len);
		break;
#endif
	}
}

#define http_ob_etaghandler(o, l, ho, hl, m) _http_ob_etaghandler((o), (l), (ho), (hl), (m) TSRMLS_CC)
extern void _http_ob_etaghandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC);

#define http_etag(p, l, m) _http_etag((p), (l), (m) TSRMLS_CC)
PHP_HTTP_API char *_http_etag(const void *data_ptr, size_t data_len, http_send_mode data_mode TSRMLS_DC);

#define http_last_modified(p, m) _http_last_modified((p), (m) TSRMLS_CC)
PHP_HTTP_API time_t _http_last_modified(const void *data_ptr, http_send_mode data_mode TSRMLS_DC);

#define http_match_last_modified(entry, modified) _http_match_last_modified_ex((entry), (modified), 1 TSRMLS_CC)
#define http_match_last_modified_ex(entry, modified, ep) _http_match_last_modified_ex((entry), (modified), (ep) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_match_last_modified_ex(const char *entry, time_t t, zend_bool enforce_presence TSRMLS_DC);

#define http_match_etag(entry, etag) _http_match_etag_ex((entry), (etag), 1 TSRMLS_CC)
#define http_match_etag_ex(entry, etag, ep) _http_match_etag_ex((entry), (etag), (ep) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_match_etag_ex(const char *entry, const char *etag, zend_bool enforce_presence TSRMLS_DC);

#define http_cache_last_modified(l, s, cc, ccl) _http_cache_last_modified((l), (s), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_last_modified(time_t last_modified, time_t send_modified, const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_cache_etag(e, el, cc, ccl) _http_cache_etag((e), (el), (cc), (ccl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, size_t etag_len, const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_start_ob_etaghandler() _http_start_ob_etaghandler(TSRMLS_C)
PHP_HTTP_API STATUS _http_start_ob_etaghandler(TSRMLS_D);
#define http_interrupt_ob_etaghandler() _http_interrupt_ob_etaghandler(TSRMLS_C)
PHP_HTTP_API zend_bool _http_interrupt_ob_etaghandler(TSRMLS_D);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

