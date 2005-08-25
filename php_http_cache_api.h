/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_CACHE_API_H
#define PHP_HTTP_CACHE_API_H

#include "php_http_std_defs.h"
#include "php_http.h"
#include "php_http_api.h"
#include "php_http_send_api.h"

#include "zend_ini.h"

#ifdef HAVE_LIBMHASH
#	include <mhash.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

typedef enum {
	HTTP_ETAG_MD5 = -2,
	HTTP_ETAG_SHA1 = -1,
	HTTP_ETAG_MHASH = 0,
} http_etag_mode;

#ifdef HAVE_LIBMHASH
static void *http_etag_alloc_mhash_digest(size_t size)
{
	return emalloc(size);
}
#endif

#define http_etag_digest(d, l) _http_etag_digest((d), (l) TSRMLS_CC)
static inline char *_http_etag_digest(const unsigned char *digest, int len TSRMLS_DC)
{
	int i;
	char *hex = emalloc(len * 2 + 1);
	char *ptr = hex;
	
	/* optimize this --
		look at apache's make_etag */
	for (i = 0; i < len; ++i) {
		sprintf(ptr, "%02x", digest[i]);
		ptr += 2;
	}
	*ptr = '\0';
	
	return hex;
}

#define http_etag_init() _http_etag_init(TSRMLS_C)
static inline void *_http_etag_init(TSRMLS_D)
{
	void *ctx;
	long mode = INI_INT("http.etag_mode");
	
	switch (mode)
	{
		case HTTP_ETAG_SHA1:
			PHP_SHA1Init(ctx = emalloc(sizeof(PHP_SHA1_CTX)));
		break;
		
		case HTTP_ETAG_MD5:
invalid_flag:
			PHP_MD5Init(ctx = emalloc(sizeof(PHP_MD5_CTX)));
		break;
		
		default:
		{
#ifdef HAVE_LIBMHASH
			if ((mode >= 0) && (mode <= mhash_count())) {
				ctx = mhash_init(mode);
			}
			if ((!ctx) || (ctx == MHASH_FAILED))
#endif
			{
				HTTP_G(etag).mode = HTTP_ETAG_MD5;
				goto invalid_flag;
			}
		}
		break;
	}
	
	return ctx;
}

#define http_etag_finish(c) _http_etag_finish((c) TSRMLS_CC)
static inline char *_http_etag_finish(void *ctx TSRMLS_DC)
{
	char *etag = NULL;
	unsigned char digest[20];
	long mode = INI_INT("http.etag_mode");
	
	switch (mode)
	{
		case HTTP_ETAG_SHA1:
			PHP_SHA1Final(digest, ctx);
			etag = http_etag_digest(digest, 20);
			efree(ctx);
		break;
		
		case HTTP_ETAG_MD5:
			PHP_MD5Final(digest, ctx);
			etag = http_etag_digest(digest, 16);
			efree(ctx);
		break;
		
		default:
		{
#ifdef HAVE_LIBMHASH
			unsigned char *mhash_digest = mhash_end_m(ctx, http_etag_alloc_mhash_digest);
			etag = http_etag_digest(mhash_digest, mhash_get_block_size(mode));
			efree(mhash_digest);
#endif
		}
		break;
	}
	
	return etag;
}

#define http_etag_update(c, d, l) _http_etag_update((c), (d), (l) TSRMLS_CC)
static inline void _http_etag_update(void *ctx, const char *data_ptr, size_t data_len TSRMLS_DC)
{
	switch (INI_INT("http.etag_mode"))
	{
		case HTTP_ETAG_SHA1:
			PHP_SHA1Update(ctx, data_ptr, data_len);
		break;
		
		case HTTP_ETAG_MD5:
			PHP_MD5Update(ctx, data_ptr, data_len);
		break;
		
		default:
#ifdef HAVE_LIBMHASH
			mhash(ctx, data_ptr, data_len);
#endif
		break;
	}
}

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

#define http_ob_etaghandler(o, l, ho, hl, m) _http_ob_etaghandler((o), (l), (ho), (hl), (m) TSRMLS_CC)
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

