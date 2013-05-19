/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

#ifdef PHP_HTTP_HAVE_HASH
#	include "php_hash.h"
#endif

#include <ext/standard/crc32.h>
#include <ext/standard/sha1.h>
#include <ext/standard/md5.h>

PHP_HTTP_API php_http_etag_t *php_http_etag_init(const char *mode TSRMLS_DC)
{
	void *ctx;
	php_http_etag_t *e;

	if (mode && (!strcasecmp(mode, "crc32b"))) {
		ctx = emalloc(sizeof(uint));
		*((uint *) ctx) = ~0;
	} else if (mode && !strcasecmp(mode, "sha1")) {
		PHP_SHA1Init(ctx = emalloc(sizeof(PHP_SHA1_CTX)));
	} else if (mode && !strcasecmp(mode, "md5")) {
		PHP_MD5Init(ctx = emalloc(sizeof(PHP_MD5_CTX)));
	} else {
#ifdef PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
			ctx = emalloc(eho->context_size);
			eho->hash_init(ctx);
		} else
#endif
		return NULL;
	}

	e = emalloc(sizeof(*e));
	e->ctx = ctx;
	e->mode = estrdup(mode);
	TSRMLS_SET_CTX(e->ts);

	return e;
}

PHP_HTTP_API char *php_http_etag_finish(php_http_etag_t *e)
{
	unsigned char digest[128] = {0};
	char *etag = NULL;

	if (!strcasecmp(e->mode, "crc32b")) {
		unsigned char buf[4];

		*((uint *) e->ctx) = ~*((uint *) e->ctx);
		buf[0] = ((unsigned char *) e->ctx)[3];
		buf[1] = ((unsigned char *) e->ctx)[2];
		buf[2] = ((unsigned char *) e->ctx)[1];
		buf[3] = ((unsigned char *) e->ctx)[0];
		etag = php_http_etag_digest(buf, 4);
	} else if ((!strcasecmp(e->mode, "sha1"))) {
		PHP_SHA1Final(digest, e->ctx);
		etag = php_http_etag_digest(digest, 20);
	} else if ((!strcasecmp(e->mode, "md5"))) {
		PHP_MD5Final(digest, e->ctx);
		etag = php_http_etag_digest(digest, 16);
	} else {
#ifdef PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if (e->mode && (eho = php_hash_fetch_ops(e->mode, strlen(e->mode)))) {
			eho->hash_final(digest, e->ctx);
			etag = php_http_etag_digest(digest, eho->digest_size);
		}
#endif
	}

	efree(e->ctx);
	efree(e->mode);
	efree(e);

	return etag;
}

PHP_HTTP_API size_t php_http_etag_update(php_http_etag_t *e, const char *data_ptr, size_t data_len)
{
	if (!strcasecmp(e->mode, "crc32b")) {
		uint i, c = *((uint *) e->ctx);
		for (i = 0; i < data_len; ++i) {
			CRC32(c, data_ptr[i]);
		}
		*((uint *) e->ctx) = c;
	} else if ((!strcasecmp(e->mode, "sha1"))) {
		PHP_SHA1Update(e->ctx, (const unsigned char *) data_ptr, data_len);
	} else if ((!strcasecmp(e->mode, "md5"))) {
		PHP_MD5Update(e->ctx, (const unsigned char *) data_ptr, data_len);
	} else {
#ifdef PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if (e->mode && (eho = php_hash_fetch_ops(e->mode, strlen(e->mode)))) {
			eho->hash_update(e->ctx, (const unsigned char *) data_ptr, data_len);
		}
#endif
	}

	return data_len;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

