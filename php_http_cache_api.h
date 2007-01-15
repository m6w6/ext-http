/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_CACHE_API_H
#define PHP_HTTP_CACHE_API_H

#include "php_http_send_api.h"

#include "ext/standard/crc32.h"
#include "ext/standard/sha1.h"
#include "ext/standard/md5.h"

#ifdef HTTP_HAVE_HASH
#	include "php_hash.h"
#endif

#define http_etag_digest(d, l) _http_etag_digest((d), (l))
static inline char *_http_etag_digest(const unsigned char *digest, int len)
{
	static const char hexdigits[17] = "0123456789abcdef";
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

#define http_etag_init() _http_etag_init(TSRMLS_C)
static inline void *_http_etag_init(TSRMLS_D)
{
	void *ctx = NULL;
	char *mode = HTTP_G->etag.mode;
	
#ifdef HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;
	
	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		ctx = emalloc(eho->context_size);
		eho->hash_init(ctx);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		ctx = emalloc(sizeof(uint));
		*((uint *) ctx) = ~0;
	} else if (mode && !strcasecmp(mode, "sha1")) {
		PHP_SHA1Init(ctx = emalloc(sizeof(PHP_SHA1_CTX)));
	} else {
		PHP_MD5Init(ctx = emalloc(sizeof(PHP_MD5_CTX)));
	}
	
	return ctx;
}

#define http_etag_finish(c) _http_etag_finish((c) TSRMLS_CC)
static inline char *_http_etag_finish(void *ctx TSRMLS_DC)
{
	unsigned char digest[128] = {0};
	char *etag = NULL, *mode = HTTP_G->etag.mode;
	
#ifdef HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;
	
	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		eho->hash_final(digest, ctx);
		etag = http_etag_digest(digest, eho->digest_size);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		*((uint *) ctx) = ~*((uint *) ctx);
		etag = http_etag_digest((const unsigned char *) ctx, sizeof(uint));
	} else if (mode && (!strcasecmp(mode, "sha1"))) {
		PHP_SHA1Final(digest, ctx);
		etag = http_etag_digest(digest, 20);
	} else {
		PHP_MD5Final(digest, ctx);
		etag = http_etag_digest(digest, 16);
	}
	efree(ctx);
	
	return etag;
}

#define http_etag_update(c, d, l) _http_etag_update((c), (d), (l) TSRMLS_CC)
static inline void _http_etag_update(void *ctx, const char *data_ptr, size_t data_len TSRMLS_DC)
{
	char *mode = HTTP_G->etag.mode;
#ifdef HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;
	
	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		eho->hash_update(ctx, (const unsigned char *) data_ptr, data_len);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		uint i, c = *((uint *) ctx);
		for (i = 0; i < data_len; ++i) {
			CRC32(c, data_ptr[i]);
		}
		*((uint *)ctx) = c;
	} else if (mode && (!strcasecmp(mode, "sha1"))) {
		PHP_SHA1Update(ctx, (const unsigned char *) data_ptr, data_len);
	} else {
		PHP_MD5Update(ctx, (const unsigned char *) data_ptr, data_len);
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

